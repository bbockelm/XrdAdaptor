#ifndef Utilities_XrdAdaptor_XrdRequest_h
#define Utilities_XrdAdaptor_XrdRequest_h

#include <vector>

#include <boost/utility.hpp>
#include <tbb/task.h>
#include <XrdCl/XrdClMessageUtils.hh>

#include "Utilities/StorageFactory/interface/Storage.h"

namespace XrdAdaptor {

class Source;

class ClientRequest : boost::noncopyable, public XrdCl::ResponseHandler, public tbb::task {

friend class Source;

public:
    ClientRequest(void *into, IOSize size, IOOffset off)
        : m_into(into),
          m_size(size),
          m_off(off),
          m_iolist(nullptr),
          m_status(nullptr)
    {
        set_ref_count(1);
    }

    ClientRequest(std::shared_ptr<std::vector<IOPosBuffer> > iolist)
        : m_into(nullptr),
          m_size(0),
          m_off(0),
          m_iolist(iolist),
          m_status(nullptr)
    {
        set_ref_count(1);
    }

    void fulfill()
    {
        wait_for_all();
    }

    std::shared_ptr<XrdCl::XRootDStatus> getStatus()
    {
        return m_status;
    }

    virtual tbb::task* execute() override {return nullptr;}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override
    {
        m_status.reset(status);
        delete response;
        decrement_ref_count();
    }

private:
    void *m_into;
    IOSize m_size;
    IOOffset m_off;
    std::shared_ptr<std::vector<IOPosBuffer> > m_iolist;
    std::shared_ptr<XrdCl::XRootDStatus> m_status;

};

class RequestList {

public:
    RequestList(const std::vector<ClientRequest> &);
/*
    pop_front();
    pop_back();
    push_front();
    push_back();
*/
};

}

#endif

