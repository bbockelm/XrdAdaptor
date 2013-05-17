
#include <iostream>

#include "XrdRequest.h"
#include "XrdRequestManager.h"

using namespace XrdAdaptor;

XrdAdaptor::ClientRequest::~ClientRequest() {}

void 
XrdAdaptor::ClientRequest::HandleResponse(XrdCl::XRootDStatus *stat, XrdCl::AnyObject *resp)
{
    std::unique_ptr<XrdCl::AnyObject> response(resp);
    std::unique_ptr<XrdCl::XRootDStatus> status(stat);
    {
        QualityMetricWatch qmw;
        m_qmw.swap(qmw);
    }
    if (status->IsOK() && resp)
    {
        if (m_into)
        {
            XrdCl::ChunkInfo *read_info;
            response->Get(read_info);
            m_promise.set_value(read_info->length);
        }
        else
        {
            XrdCl::VectorReadInfo *read_info;
            response->Get(read_info);
            m_promise.set_value(read_info->GetSize());
        }
    }
    else
    {
        m_failure_count++;
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

