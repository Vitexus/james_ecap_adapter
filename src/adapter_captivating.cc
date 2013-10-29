#include "james_ecap.h"
#include <iostream>
#include <fstream>
#include <libecap/common/registry.h>
#include <libecap/common/errors.h>
#include <libecap/common/message.h>
#include <libecap/common/header.h>
#include <libecap/common/names.h>
#include <libecap/common/named_values.h>
#include <libecap/common/config.h>
#include <libecap/host/host.h>
#include <libecap/adapter/service.h>
#include <libecap/adapter/xaction.h>
#include <libecap/host/xaction.h>
#include <libecap/common/names.h>
#include <mysql++/mysql++.h>
#include <iomanip>
#include <libconfig.h++>
#include <time.h>
#include <libecap/common/body.h>


#define CAPTIVE_TIMEOUT 3600




namespace Adapter { // not required, but adds clarity

    using libecap::size_type;

    class Service : public libecap::adapter::Service {
    public:
        // About
        virtual std::string uri() const; // unique across all vendors
        virtual std::string tag() const; // changes with version and config
        virtual void describe(std::ostream &os) const; // free-format info

        // Configuration
        virtual void configure(const libecap::Options &cfg);
        virtual void reconfigure(const libecap::Options &cfg);
        virtual void setOne(const libecap::Name &name, const libecap::Area &valArea);
        virtual void loadConfig(std::string conffile);

        // Lifecycle
        virtual void start(); // expect makeXaction() calls
        virtual void stop(); // no more makeXaction() calls until start()
        virtual void retire(); // no more makeXaction() calls

        // Scope (XXX: this may be changed to look at the whole header)
        virtual bool wantsUrl(const char *url) const;

        // Work
        virtual libecap::adapter::Xaction *makeXaction(libecap::host::Xaction *hostx);

    public:
        // Configuration storage

        std::string victim; // the text we want to replace
        std::string replacement; // what the replace the victim with
        mysqlpp::Connection conn;
        std::string dbhost;
        std::string dbname;
        std::string dblogin;
        std::string dbpassw;

    };

    // Calls Service::setOne() for each host-provided configuration option.
    // See Service::configure().

    class Cfgtor : public libecap::NamedValueVisitor {
    public:

        Cfgtor(Service &aSvc) : svc(aSvc) {
        }

        virtual void visit(const libecap::Name &name, const libecap::Area &value) {
            svc.setOne(name, value);
        }
        Service &svc;
    };

    class Xaction : public libecap::adapter::Xaction {
    public:
        Xaction(libecap::shared_ptr<Service> s, libecap::host::Xaction *x);
        virtual ~Xaction();

        // meta-information for the host transaction
        virtual const libecap::Area option(const libecap::Name &name) const;
        virtual void visitEachOption(libecap::NamedValueVisitor &visitor) const;

        // lifecycle
        virtual void start();
        virtual void stop();

        // adapted body transmission control
        virtual void abDiscard();
        virtual void abMake();
        virtual void abMakeMore();
        virtual void abStopMaking();

        // adapted body content extraction and consumption
        virtual libecap::Area abContent(size_type offset, size_type size);
        virtual void abContentShift(size_type size);

        // virgin body state notification
        virtual void noteVbContentDone(bool atEnd);
        virtual void noteVbContentAvailable();

        // libecap::Callable API, via libecap::host::Xaction
        virtual bool callable() const;
        mysqlpp::Connection sqlConn;
    protected:

        void stopVb(); // stops receiving vb (if we are receiving it)
        libecap::host::Xaction *lastHostCall(); // clears hostx

    private:
        libecap::shared_ptr<const Service> sharedService; // configuration access
        libecap::host::Xaction *hostx; // Host transaction rep

        std::string buffer; // for content adaptation
        std::string clientIP; //client IP

        typedef enum {
            opUndecided, opOn, opComplete, opNever
        } OperationState;
        OperationState receivingVb;
        OperationState sendingAb;

        typedef enum {
            stBlocked, stAllowed
        } CaptiveState;
        CaptiveState capState;
        libecap::Area ResponsePage(void);
        void cnStart(void);

    };

    static const std::string CfgErrorPrefix =
            "Captivating Adapter: configuration error: ";

} // namespace Adapter

std::string Adapter::Service::uri() const {
    return "ecap://murka.cz/james/captivating";
}

std::string Adapter::Service::tag() const {
    return PACKAGE_VERSION;
}

void Adapter::Service::describe(std::ostream &os) const {
    os << "A captivating adapter from " << PACKAGE_NAME << " v" << PACKAGE_VERSION;
}

void Adapter::Service::configure(const libecap::Options &cfg) {
    FUNCENTER();
    Cfgtor cfgtor(*this);
    cfg.visitEachOption(cfgtor);
}

void Adapter::Service::reconfigure(const libecap::Options &) {
    FUNCENTER();
    //loadConfig();
}

void Adapter::Service::loadConfig(std::string conffile) {
    FUNCENTER();

    char _buff[1024], tag[24], val[100];
    std::ifstream cfg(conffile.c_str());

    while (!cfg.eof()) {
        cfg.getline(_buff, 1024);
        if (_buff[0] != '#' && _buff[0] != '\n') {
            //puts(_buff);
            sscanf(_buff, "%s = \"%s", tag, val);
            val[strlen(val) - 2] = 0;

            if (!strcmp(tag, "dbhost")) {
                if (dbhost.empty())
                    dbhost.assign(val);
            }
            if (!strcmp(tag, "dbname")) {
                if (dbname.empty())
                    dbname.assign(val);
            }
            if (!strcmp(tag, "dblogin")) {
                if (dblogin.empty())
                    dblogin.assign(val);
            }
            if (!strcmp(tag, "dbpassw")) {
                if (dbpassw.empty())
                    dbpassw.assign(val);
            }
        }
    }
    cfg.close();
}

void Adapter::Service::setOne(const libecap::Name &name, const libecap::Area &valArea) {
    FUNCENTER();
    const std::string value = valArea.toString();

    if (name.image() == "config") {
        loadConfig(value);
    } else {
        if (name.assignedHostId())
            ; // skip host-standard options we do not know or care about
        else
            throw libecap::TextException(Adapter::CfgErrorPrefix +
                "unsupported configuration parameter: " + name.image());

    }
}

void Adapter::Service::start() {
    FUNCENTER();
    libecap::adapter::Service::start();
    // custom code would go here, but this service does not have one
}

void Adapter::Service::stop() {
    FUNCENTER();
    // custom code would go here, but this service does not have one
    libecap::adapter::Service::stop();
}

void Adapter::Service::retire() {
    FUNCENTER();
    // custom code would go here, but this service does not have one
    libecap::adapter::Service::stop();
}

bool Adapter::Service::wantsUrl(const char *url) const {
    FUNCENTER();
    return true; // no-op is applied to all messages
}

libecap::adapter::Xaction *Adapter::Service::makeXaction(libecap::host::Xaction *hostx) {
    FUNCENTER();
    return new Adapter::Xaction(std::tr1::static_pointer_cast<Service>(self), hostx);
}

void Adapter::Xaction::cnStart(void) {
    FUNCENTER();

}

/** constructor Xaction */
Adapter::Xaction::Xaction(libecap::shared_ptr<Service> aService,
        libecap::host::Xaction *x) :
sharedService(aService), hostx(x), receivingVb(opUndecided), sendingAb(opUndecided) {
    FUNCENTER();
    //    conn = &aService->conn;
    sqlConn = sharedService->conn;
}

Adapter::Xaction::~Xaction() {
    FUNCENTER();
    if (libecap::host::Xaction * x = hostx) {
        hostx = 0;
        x->adaptationAborted();
    }

}

const libecap::Area Adapter::Xaction::option(const libecap::Name &) const {
    FUNCENTER();
    return libecap::Area(); // this transaction has no meta-information
}

void Adapter::Xaction::visitEachOption(libecap::NamedValueVisitor &) const {
    FUNCENTER();
    // this transaction has no meta-information to pass to the visitor
}

char* itoa(int i, char b[]) {
    char const digit[] = "0123456789";
    char* p = b;
    if (i < 0) {
        *p++ = '-';
        i = -1;
    }
    int shifter = i;
    do { //Move to where representation ends
        ++p;
        shifter = shifter / 10;
    } while (shifter);
    *p = '\0';
    do { //Move back, inserting digits as u go
        *--p = digit[i % 10];
        i = i / 10;
    } while (i);
    return b;
}

libecap::Area Adapter::Xaction::ResponsePage(void) {
    FUNCENTER();
    std::string errmsg = "<HTML><HEAD><TITLE>";
    if (capState == stAllowed) {
        errmsg += "Success";
    } else {
        errmsg += "Blocked";
    }
    errmsg += "</TITLE></HEAD><BODY>";
    if (capState == stAllowed) {
        errmsg += "Success";
    } else {
        errmsg += "Blocked";
    }
    errmsg += "</BODY></HTML>";
    return libecap::Area::FromTempString(errmsg);
}

/* Zacatek procesu*/
void Adapter::Xaction::start() {
    FUNCENTER();
    Must(hostx);
    if (hostx->virgin().body()) {
        receivingVb = opOn;
        hostx->vbMake(); // ask host to supply virgin body
    } else {
        // we are not interested in vb if there is not one
        receivingVb = opNever;
    }



    int cn = 1;
    char cntime[] = "0000-00-00 00:00:00";
    time_t cntimestamp = 0;
    mysqlpp::Row row;

    libecap::host::Xaction *x = hostx;
    libecap::Area area = x->option(libecap::metaClientIp);
    clientIP.assign(area.start);

    if (!sqlConn.connected()) {
        // check for post-configuration errors and inconsistencies
        if (sqlConn.connect(sharedService->dbname.c_str(), sharedService->dbhost.c_str(), sharedService->dblogin.c_str(), sharedService->dbpassw.c_str())) {
            std::cout << "SQL reconnect" << std::endl;
        } else {
            std::cerr << "DB connection failed: " << sqlConn.error() << std::endl;
        }

    }

    std::string ip_query = "SELECT cn,cntime, UNIX_TIMESTAMP(cntime) AS cntimestamp FROM `clients` WHERE `ip`='";
    ip_query.append(clientIP).append("'");

    mysqlpp::Query query = sqlConn.query(ip_query);
    mysqlpp::StoreQueryResult res = query.store(); //Problem

    if (res) {
        mysqlpp::StoreQueryResult::const_iterator it;
        for (it = res.begin(); it != res.end(); ++it) {
            row = *it;
            cn = row[0];
            strcpy(cntime, row[1]);
            cntimestamp = atoi(row[2]);
        }


    } else {
        std::cerr << "Failed to get item list: " << query.error() << std::endl;

        //        mysqli_query($link, "INSERT INTO clients SET `ip`='" . $client_ip . "',`starttime`=NOW(),`time`=NOW(),`enabled`=0,`cn`=0,`url`='" . $_SERVER['SERVER_NAME '] . $_SERVER['REQUEST_URI '] . "'");
        std::string ins_query = "INSERT INTO clients SET `ip`='";
        ins_query.append(clientIP).append("',`starttime`=NOW(),`time`=NOW(),`enabled`=0,`cn`=0 ");
        mysqlpp::Query query2 = sqlConn.query(ins_query);
        query2.exec(); //Problem

    }


    if (strlen(cntime) && strcmp(cntime, "0000-00-00 00:00:00")) {
        if (time(NULL) - cntimestamp < CAPTIVE_TIMEOUT) {
            cn = row[0] + 1;
        }
    }

    char upquery[200];
    sprintf(upquery, "UPDATE `clients` SET `cntime`=NOW(), `cn` = %d WHERE `ip`='%s'", cn, clientIP.c_str());
    mysqlpp::Query query3 = sqlConn.query(upquery);
    query3.exec();

    if (cn % 2 == 0) {
        capState = stAllowed;
    } else {
        capState = stBlocked;
    }

    /* adapt message header */

    libecap::shared_ptr<libecap::Message> adapted = hostx->virgin().clone();
    Must(adapted != 0);

    // delete ContentLength header because we may change the length
    // unknown length may have performance implications for the host
    adapted->header().removeAny(libecap::headerContentLength);

    // add a custom header
    static const libecap::Name name("X-Ecap");
    const libecap::Header::Value value =
            libecap::Area::FromTempString(libecap::MyHost().uri());
    adapted->header().add(name, value);

    if (adapted->header().hasAny(libecap::Name("Accept-Encoding"))) //Nekomprimovat!
        adapted->header().removeAny(libecap::Name("Accept-Encoding"));

    const libecap::Name contentname("Content-Type");
    const libecap::Name disp("Content-Disposition");
    const libecap::Header::Value htmlvalue = libecap::Area::FromTempString("text/html");
    adapted->header().removeAny(disp);
    adapted->header().removeAny(contentname);
    adapted->header().add(contentname, htmlvalue);



    // Add Warning header to response, according to RFC 2616 14.46
    static const libecap::Name warningName("Warning");
    const libecap::Header::Value warningValue = libecap::Area::FromTempString("214 Transformation applied");
    adapted->header().add(warningName, warningValue);

    if (!adapted->body()) {
        sendingAb = opNever; // there is nothing to send
        lastHostCall()->useAdapted(adapted);
    } else {
        hostx->useAdapted(adapted);
    }
}

void Adapter::Xaction::stop() {
    FUNCENTER();
    hostx = 0;
    // the caller will delete
}

void Adapter::Xaction::abDiscard() {
    FUNCENTER();
    Must(sendingAb == opUndecided); // have not started yet
    sendingAb = opNever;
    // we do not need more vb if the host is not interested in ab
    stopVb();
}

void Adapter::Xaction::abMake() {
    FUNCENTER();
    Must(sendingAb == opUndecided); // have not yet started or decided not to send
    Must(hostx->virgin().body()); // that is our only source of ab content

    // we are or were receiving vb
    Must(receivingVb == opOn || receivingVb == opComplete);

    sendingAb = opOn;
    if (!buffer.empty())
        hostx->noteAbContentAvailable();
}

void Adapter::Xaction::abMakeMore() {
    FUNCENTER();
    Must(receivingVb == opOn); // a precondition for receiving more vb
    hostx->vbMakeMore();
}

void Adapter::Xaction::abStopMaking() {
    FUNCENTER();
    sendingAb = opComplete;
    // we do not need more vb if the host is not interested in more ab
    stopVb();
}

libecap::Area Adapter::Xaction::abContent(size_type offset, size_type size) {
    Must(sendingAb == opOn || sendingAb == opComplete);
    return ResponsePage();
    return libecap::Area::FromTempString(buffer.substr(offset, size));
}

void Adapter::Xaction::abContentShift(size_type size) {
    FUNCENTER();
    Must(sendingAb == opOn || sendingAb == opComplete);
    buffer.erase(0, size);
}

void Adapter::Xaction::noteVbContentDone(bool atEnd) {
    FUNCENTER();
    Must(receivingVb == opOn);
    receivingVb = opComplete;
    if (sendingAb == opOn) {
        hostx->noteAbContentDone(atEnd);
        sendingAb = opComplete;
    }

    cnStart();

}

void Adapter::Xaction::noteVbContentAvailable() {
    FUNCENTER();
    Must(receivingVb == opOn);

    const libecap::Area vb = hostx->vbContent(0, libecap::nsize); // get all vb
    std::string chunk = vb.toString(); // expensive, but simple
    hostx->vbContentShift(vb.size); // we have a copy; do not need vb any more

    //    buffer += chunk; // buffer what we got

    if (sendingAb == opOn)
        hostx->noteAbContentAvailable();
}

bool Adapter::Xaction::callable() const {
    FUNCENTER();
    return hostx != 0; // no point to call us if we are done
}

// tells the host that we are not interested in [more] vb
// if the host does not know that already

void Adapter::Xaction::stopVb() {
    FUNCENTER();
    if (receivingVb == opOn) {
        hostx->vbStopMaking();
        receivingVb = opComplete;
    } else {
        // we already got the entire body or refused it earlier
        Must(receivingVb != opUndecided);
    }
}

// this method is used to make the last call to hostx transaction
// last call may delete adapter transaction if the host no longer needs it
// TODO: replace with hostx-independent "done" method

libecap::host::Xaction *Adapter::Xaction::lastHostCall() {
    FUNCENTER();
    libecap::host::Xaction *x = hostx;
    Must(x);
    hostx = 0;
    return x;
}

// create the adapter and register with libecap to reach the host application
static const bool Registered = (libecap::RegisterService(new Adapter::Service), true);
