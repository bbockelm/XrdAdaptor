
#include <iostream>
#include <assert.h>

#include "XrdCl/XrdClFile.hh"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "XrdSource.h"
#include "XrdRequest.h"
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
Source::handle(std::shared_ptr<ClientRequest> c)
{
    std::cout << "Reading from " << ID() << ", quality " << m_qm->get() << std::endl;
    c->m_self_reference = c;
    c->m_source_id = ID();
    m_qm->startWatch(c->m_qmw);
    if (c->m_into)
    {
        // See notes in ClientRequest definition to understand this voodoo.
        m_fh->Read(c->m_off, c->m_size, c->m_into, c.get());
    }
    else
    {
        XrdCl::ChunkList cl;
        cl.reserve(c->m_iolist->size());
        for (const auto & it : *c->m_iolist)
        {
            XrdCl::ChunkInfo ci(it.offset(), it.size(), it.data());
            cl.emplace_back(ci);
        }
        m_fh->VectorRead(cl, nullptr, c.get());
    }
}

