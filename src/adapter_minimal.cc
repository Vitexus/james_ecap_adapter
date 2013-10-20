#include "sample.h"
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
        std::string clientIP; //client IP
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

    // a minimal adapter transaction

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

        virtual void abDiscard() {
            noBodySupport();
        }

        virtual void abMake() {
            noBodySupport();
        }

        virtual void abMakeMore() {
            noBodySupport();
        }

        virtual void abStopMaking() {
            noBodySupport();
        }

        // adapted body content extraction and consumption

        virtual libecap::Area abContent(libecap::size_type, libecap::size_type) {
            noBodySupport();
            return libecap::Area();
        }

        virtual void abContentShift(libecap::size_type) {
            noBodySupport();
        }

        // virgin body state notification

        virtual void noteVbContentDone(bool) {
            noBodySupport();
        }

        virtual void noteVbContentAvailable() {
            noBodySupport();
        }

        // libecap::Callable API, via libecap::host::Xaction
        virtual bool callable() const;

    protected:
        void noBodySupport() const;

    private:
        libecap::shared_ptr<const Service> service; // configuration access
        libecap::host::Xaction *hostx; // Host transaction rep

        std::string buffer; // for content adaptation

        typedef enum {
            opUndecided, opOn, opComplete, opNever
        } OperationState;
        OperationState receivingVb;
        OperationState sendingAb;
    };
    static const std::string CfgErrorPrefix = "Minimal Adapter: configuration error: ";
} // namespace Adapter

std::string Adapter::Service::uri() const {
    return "ecap://e-cap.org/ecap/services/sample/minimal";
}

std::string Adapter::Service::tag() const {
    return PACKAGE_VERSION;
}

void Adapter::Service::describe(std::ostream &os) const {
    os << "A minimal adapter from " << PACKAGE_NAME << " v" << PACKAGE_VERSION;
}

void Adapter::Service::configure(const libecap::Options &cfg) {
    Cfgtor cfgtor(*this);
    cfg.visitEachOption(cfgtor);

    // check for post-configuration errors and inconsistencies

    if (conn.connect(dbname.c_str(), dbhost.c_str(), dblogin.c_str(), dbpassw.c_str())) {
        //All Ok
    } else {
        std::cerr << "DB connection failed: " << conn.error() << std::endl;
    }

    if (!conn.connected()) {
        throw libecap::TextException(Adapter::CfgErrorPrefix + "database not connected");
    }
}

void Adapter::Service::reconfigure(const libecap::Options &) {
    // this service is not configurable
}

using namespace std;
using namespace libconfig;

void Adapter::Service::loadConfig(string conffile) {

    char _buff[1024], tag[24], val[100];
    ifstream cfg(conffile.c_str());

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
    const std::string value = valArea.toString();

    if (name == "config") {
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
    libecap::adapter::Service::start();
}

void Adapter::Service::stop() {
    // custom code would go here, but this service does not have one
    libecap::adapter::Service::stop();
}

void Adapter::Service::retire() {
    // custom code would go here, but this service does not have one
    libecap::adapter::Service::stop();
}

bool Adapter::Service::wantsUrl(const char *url) const {
    return true; // minimal adapter is applied to all messages
}

libecap::adapter::Xaction *Adapter::Service::makeXaction(libecap::host::Xaction *hostx) {
    return new Adapter::Xaction(std::tr1::static_pointer_cast<Service>(self),
            hostx);
}

/** constructor Xaction */
Adapter::Xaction::Xaction(libecap::shared_ptr<Service> aService,
        libecap::host::Xaction *x) :
service(aService),
hostx(x),
receivingVb(opUndecided), sendingAb(opUndecided) {
}

Adapter::Xaction::~Xaction() {
    if (libecap::host::Xaction * x = hostx) {
        hostx = 0;
        x->adaptationAborted();
    }
}

const libecap::Area Adapter::Xaction::option(const libecap::Name &) const {
    return libecap::Area(); // this transaction has no meta-information
}

void Adapter::Xaction::visitEachOption(libecap::NamedValueVisitor &) const {
    // this transaction has no meta-information to pass to the visitor
}

void Adapter::Xaction::start() {
    Must(hostx);
    // make this adapter non-callable
    libecap::host::Xaction *x = hostx;
    hostx = 0;

    libecap::Area area = x->option(libecap::metaClientIp);
    std::string update_query = "UPDATE clients SET `time`=NOW() WHERE ip=\"";
    update_query.append(area.start);
    update_query.append("\"");

    //    mysqlpp::Query query = conn.query(update_query.c_str());
    //    query.execute();


    // tell the host to use the virgin message
    x->useVirgin();
}

void Adapter::Xaction::stop() {
    hostx = 0;
    // the caller will delete
}

bool Adapter::Xaction::callable() const {
    return hostx != 0; // no point to call us if we are done
}

void Adapter::Xaction::noBodySupport() const {
    Must(!"must not be called: minimal adapter offers no body support");
    // not reached
}

// create the adapter and register with libecap to reach the host application
static const bool Registered = (libecap::RegisterService(new Adapter::Service), true);


