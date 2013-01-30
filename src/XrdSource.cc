
#include "XrdSource.h"
#include "QualityMetric.h"

using namespace XrdAdaptor;

Source::Source(timespec now, std::unique_ptr<XrdFile> fh)
    : m_fh(fh),
      m_id(fh->GetDataServer()),
      m_qm(QualityMetricFactory::get(now, m_id))
{
    m_buffer.reserve(MAX_REQUEST)

    m_file.reset(new XrdCl::File());
    XrdCl::XRootDStatus status;
    if (! (status = m_file->Open(name, openflags, perms)).IsOK())
    {
        edm::Exception ex(edm::errors::FileOpenError);
        ex << "XrdCl::File::Open(name='" << name
           << "', flags=0x" << std::hex << openflags
           << ", permissions=0" << std::oct << perms << std::dec
           << ") => error '" << status.ToString()
           << "' (errno=" << status.errNo << ", code=" << status.code << ")";
        ex.addContext("Calling XrdFile::open()");
        addConnection(ex);
        throw ex;
    }
    XrdCl::StatInfo *statInfo = NULL;
    if (! (status = m_file->Stat(true, statInfo)).IsOK()) {
        edm::Exception ex(edm::errors::FileOpenError);
        ex << "XrdCl::File::Stat(name='" << name
           << ") => error '" << status.ToString()
           << "' (errno=" << status.errNo << ", code=" << status.code << ")";
        ex.addContext("Calling XrdFile::open()");
        addConnection(ex);
        throw ex;
    }
    assert(statInfo);
    m_size = statInfo->GetSize();
    delete(statInfo);
    m_offset = 0;
    m_close = true;
}

Source::~Source()
{
  XrdCl::XRootDStatus status;
  if (! (status = m_file->Close()).IsOK())
    edm::LogWarning("XrdFileWarning")
      << "XrdFile::close(name='" << m_name
      << "') failed with error '" << status.ToString()
      << "' (errno=" << status.errNo << ", code=" << status.code << ")";
  m_file.reset(nullptr);
}

Source::handle(RequestList &)
{
    
}
