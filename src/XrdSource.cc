
#include <assert.h>

#include "XrdCl/XrdClFile.hh"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "XrdSource.h"
#include "QualityMetric.h"

#define MAX_REQUEST 256*1024

using namespace XrdAdaptor;

Source::Source(timespec now, std::unique_ptr<XrdCl::File> fh)
    : m_id(fh->GetDataServer()),
      m_fh(std::move(fh)),
      m_qm(QualityMetricFactory::get(now, m_id))
{
    assert(m_qm.get());
    assert(m_fh.get());
    m_buffer.reserve(MAX_REQUEST);
}

Source::~Source()
{
  XrdCl::XRootDStatus status;
  if (! (status = m_fh->Close()).IsOK())
    edm::LogWarning("XrdFileWarning")
      << "Source::~Source() failed with error '" << status.ToString()
      << "' (errno=" << status.errNo << ", code=" << status.code << ")";
  m_fh.reset();
}

std::shared_ptr<XrdCl::File>
Source::getFileHandle()
{
    return m_fh;
}

void
Source::handle(RequestList &)
{
    
}
