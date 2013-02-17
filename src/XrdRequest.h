#ifndef Utilities_XrdAdaptor_XrdRequest_h
#define Utilities_XrdAdaptor_XrdRequest_h

#include <future>
#include <vector>

#include <boost/utility.hpp>
#include <XrdCl/XrdClMessageUtils.hh>

#include "Utilities/StorageFactory/interface/Storage.h"

#include "QualityMetric.h"

namespace XrdAdaptor {

class Source;

class RequestManager;

class ClientRequest : boost::noncopyable, public XrdCl::ResponseHandler {

friend class Source;

public:

    ClientRequest(RequestManager &manager, void *into, IOSize size, IOOffset off)
        : m_into(into),
          m_size(size),
          m_off(off),
          m_iolist(nullptr),
          m_manager(manager)
    {
    }

    ClientRequest(RequestManager &manager, std::shared_ptr<std::vector<IOPosBuffer> > iolist)
        : m_into(nullptr),
          m_size(0),
          m_off(0),
          m_iolist(iolist),
          m_manager(manager)
    {
    }

    virtual ~ClientRequest();

    std::future<IOSize> get_future()
    {
        return m_promise.get_future();
    }

    /**
     * Handle the response from the Xrootd server.
     */
    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

    IOSize getSize() const {return m_size;}

private:
    void *m_into;
    IOSize m_size;
    IOOffset m_off;
    std::unique_ptr<XrdCl::ChunkInfo> m_read_info;
    std::shared_ptr<std::vector<IOPosBuffer> > m_iolist;
    std::shared_ptr<XrdCl::XRootDStatus> m_status;
    RequestManager &m_manager;

    // Some explanation is due here.  When an IO is outstanding,
    // Xrootd takes a raw pointer to this object.  Hence we cannot
    // allow it to go out of scope until some indeterminate time in the
    // future.  So, while the IO is outstanding, we take a reference to
    // ourselve to prevent the object from being unexpectedly deleted.
    std::shared_ptr<ClientRequest> m_self_reference;

    std::promise<IOSize> m_promise;

    QualityMetricWatch m_qmw;
};

class RequestList : boost::noncopyable, public XrdCl::ResponseHandler {

public:

    RequestList(const std::vector<ClientRequest> &);

    /**
     * Block until all the requests are fulfilled.
     */
    void fulfill();

    /**
     * Return a status object representing the status of all the IO requests.
     * Will be null if any of the requests are not finished.
     * Will only be OK if all requests are successful.  Otherwise, this returns
     * the first available error.
     */
    std::shared_ptr<XrdCl::XRootDStatus> getStatus();

/*
    pop_front();
    pop_back();
    push_front();
    push_back();
*/
};

}

#endif

