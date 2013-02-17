
#include <iostream>

#include "XrdRequest.h"
#include "XrdRequestManager.h"

using namespace XrdAdaptor;

XrdAdaptor::ClientRequest::~ClientRequest() {}

void 
XrdAdaptor::ClientRequest::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response)
{
    m_status.reset(status);
    QualityMetricWatch qmw;
    m_qmw.swap(qmw);
    if (m_status->IsOK())
    {
        if (m_into)
        {
            XrdCl::ChunkInfo *read_info;
            response->Get(read_info);
            m_promise.set_value(read_info->length);
            delete response;
        }
        else
        {
            // TODO: set error and/or implement
        }
    }
    else
    {
        // TODO: implement recovery mechanisms.
        edm::Exception ex(edm::errors::FileReadError);
        ex << "XrdRequestManager::handle(name='" << m_manager.getFilename()
           << ") failed with error '" << status->ToString()
           << "' (errno=" << status->errNo << ", code="
           << status->code << ")";
        ex.addContext("Calling XrdRequestManager::handle()");
        // TODO: Inform request manager of failures.
        m_manager.addConnections(ex);
        m_promise.set_exception(std::make_exception_ptr(ex));
    }
    m_self_reference = nullptr;
}

