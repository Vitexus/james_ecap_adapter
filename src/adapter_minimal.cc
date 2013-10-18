#include "sample.h"
#include <iostream>
#include <libecap/common/registry.h>
#include <libecap/common/errors.h>
#include <libecap/adapter/service.h>
#include <libecap/adapter/xaction.h>
#include <libecap/host/xaction.h>
#include <libecap/common/names.h>
#include <mysql++/mysql++.h>
#include <iomanip>

namespace Adapter { // not required, but adds clarity

    class Service : public libecap::adapter::Service {
    public:
        // About
        virtual std::string uri() const; // unique across all vendors
        virtual std::string tag() const; // changes with version and config
        virtual void describe(std::ostream &os) const; // free-format info

        // Configuration
        virtual void configure(const libecap::Options &cfg);
        virtual void reconfigure(const libecap::Options &cfg);

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

    };

    // TODO: libecap should provide an adapter::HeaderOnlyXact convenience class

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

void Adapter::Service::configure(const libecap::Options &) {

}

void Adapter::Service::reconfigure(const libecap::Options &) {
    // this service is not configurable
}

void Adapter::Service::start() {
    libecap::adapter::Service::start();
    
    
    // Connect to the sample database.
    mysqlpp::Connection conn(false);
    if (conn.connect("dashboard", "localhost", "root", "pishkot")) {
        // Retrieve a subset of the sample stock table set up by resetdb
        // and display it.
        mysqlpp::Query query = conn.query("select item from stock");
        if (mysqlpp::StoreQueryResult res = query.store()) {
            std::cout << "We have:" << std::endl;
            mysqlpp::StoreQueryResult::const_iterator it;
            for (it = res.begin(); it != res.end(); ++it) {
                mysqlpp::Row row = *it;
                std::cout << '\t' << row[0] << std::endl;
            }
        }
        else {
            std::cerr << "Failed to get item list: " << query.error() << std::endl;

        }


    }
    else {
        std::cerr << "DB connection failed: " << conn.error() << std::endl;

    }    
    
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
    //service.clientIP = area.start;
    

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


