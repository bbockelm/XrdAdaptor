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
          m_read_info(nullptr),
          m_iolist(nullptr),
          m_status(nullptr)
    {
        set_ref_count(1);
    }

    ClientRequest(std::shared_ptr<std::vector<IOPosBuffer> > iolist)
        : m_into(nullptr),
          m_size(0),
          m_off(0),
          m_read_info(nullptr),
          m_iolist(iolist),
          m_status(nullptr)
    {
        set_ref_count(1);
    }

    /**
     * Block until the corresponding IO operation has completed.
     */
    void fulfill()
    {
        wait_for_all();
    }

    /**
     * Return a shared pointer to the results of the IO operation.
     * If the IO operation has not completed, then this will return
     * a nullptr.
     */
    std::shared_ptr<XrdCl::XRootDStatus> getStatus(IOSize &bytesRead)
    {
        if (m_status->IsOK())
        {
            if (m_into)
            {
                bytesRead = m_read_info->length;
            }
            else
            {
                // TODO: throw error / implement.
            }
        }
        return m_status;
    }

    /**
     * A no-op function required by the tbb::task interface.  We may
     * fill this in later if we move the source logic into this class.
     */
    virtual tbb::task* execute() override {return nullptr;}

    /**
     * Handle the response from the Xrootd server.
     */
    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override
    {
        m_self_reference.reset(nullptr);
        m_status.reset(status);
        if (m_status->IsOK())
        {
            if (m_into)
            {
                ChunkInfo *read_info;
                response->Get(read_info);
                m_read_info(read_infoi);
                response->Set( nullptr );
                delete response;
            }
            else
            {
                // TODO: set error and/or implement
            }
        }
        decrement_ref_count();
    }

private:
    void *m_into;
    IOSize m_size;
    IOOffset m_off;
    std::unique_ptr<XrdCl::ChunkInfo> m_read_info;
    std::shared_ptr<std::vector<IOPosBuffer> > m_iolist;
    std::shared_ptr<XrdCl::XRootDStatus> m_status;

    // Some explanation is due here.  When an IO is outstanding,
    // Xrootd takes a raw pointer to this object.  Hence we cannot
    // allow it to go out of scope until some indeterminate time in the
    // future.  So, while the IO is outstanding, we take a reference to
    // ourselve to prevent the object from being unexpectedly deleted.
    std::shared_ptr<ClientRequest> m_self_reference;

};

class RequestList : boost::noncopyable, public XrdCl::ResponseHandler, public tbb::task {

public:

    RequestList(const std::vector<ClientRequest> &);

    /**
     * Block until all the requests are fulfilled.
     */
    fulfill();

    /**
     * Return a status object representing the status of all the IO requests.
     * Will be null if any of the requests are not finished.
     * Will only be OK if all requests are successful.  Otherwise, this returns
     * the first available error.
     */
    std::shared_ptr<XRootDStatus> getSTatus();

    tbb::task * execute() override {return nullptr;}
/*
    pop_front();
    pop_back();
    push_front();
    push_back();
*/
};

}

#endif

