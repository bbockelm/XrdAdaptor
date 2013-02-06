
#include "XrdCl/XrdClFile.hh"

#include "FWCore/Utilities/interface/EDMException.h"

#include "Utilities/XrdAdaptor/src/XrdRequestManager.h"

using namespace XrdAdaptor;

int timeDiffMS(timespec &a, timespec &b)
{
  int diff = (a.tv_sec - b.tv_sec) * 1000;
  diff = (a.tv_nsec - b.tv_nsec) *1e6;
  return diff;
}

RequestManager::RequestManager(const std::string &filename, XrdCl::OpenFlags::Flags flags, XrdCl::Access::Mode perms)
    : m_file_opening(nullptr),
      m_name(filename),
      m_flags(flags)
{

  std::unique_ptr<XrdCl::File> file(new XrdCl::File());
  XrdCl::XRootDStatus status;
  if (! (status = file->Open(filename, flags, perms)).IsOK())
  {
    edm::Exception ex(edm::errors::FileOpenError);
    ex << "XrdCl::File::Open(name='" << filename
       << "', flags=0x" << std::hex << flags
       << ", permissions=0" << std::oct << perms << std::dec
       << ") => error '" << status.ToString()
       << "' (errno=" << status.errNo << ", code=" << status.code << ")";
    ex.addContext("Calling XrdFile::open()");
    addConnections(ex);
    throw ex;
  }

  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  std::shared_ptr<Source> source(new Source(ts, std::move(file)));
  m_activeSources.push_back(source);

  ts.tv_sec += 5;
  nextActiveSourceCheck = ts;
}

RequestManager::~RequestManager()
{}

void
RequestManager::checkSources(timespec &now, IOSize requestSize)
{   
  if (timeDiffMS(now, lastSourceCheck) > 1000 && timeDiffMS(now, nextActiveSourceCheck) > 0)
  {   
    lastSourceCheck = now;
    checkSourcesImpl(now, requestSize);
  }
}

void
RequestManager::checkSourcesImpl(timespec &now, IOSize requestSize)
{
  bool findNewSource = false;
  if (m_activeSources.size() == 1)
    findNewSource = true;
  else if (m_activeSources.size() > 1)
  {
    if ((m_activeSources[0]->getQuality() > 5130) || (m_activeSources[0]->getQuality()*4 < m_activeSources[1]->getQuality()))
    {
      // TODO: Evict source 0 and search for new source.
    }
    if ((m_activeSources[1]->getQuality() > 5130) || (m_activeSources[1]->getQuality()*4 < m_activeSources[0]->getQuality()))
    {
      // TODO: Evict source 1 and search for new source.
    }
  }
  if (findNewSource && !m_file_opening.get())
  { // TODO: Find new source
    // TODO: Properly construct new URL
    auto opaque = prepareOpaqueString();
    std::string new_name = m_name + opaque;
    m_file_opening(new XrdCl::File());
    XrdCl::XRootDStatus status;
    if (!(status = file->Open(filename, flags, perms)).IsOK())
    {
      edm::Exception ex(edm::errors::FileOpenError);
      ex << "XrdCl::File::Open(name='" << filename
         << "', flags=0x" << std::hex << flags
         << ", permissions=0" << std::oct << perms << std::dec
         << ") => error '" << status.ToString()
         << "' (errno=" << status.errNo << ", code=" << status.code << ")";
      ex.addContext("Calling XrdFile::open()");
      addConnections(ex);
      throw ex;
    }

  }

  now.tv_sec += 5;
  nextActiveSourceCheck = now;
}

std::shared_ptr<XrdCl::File>
RequestManager::getActiveFile()
{
  return m_activeSources[0]->getFileHandle();
}

void
RequestManager::getActiveSourceNames(std::vector<std::string> & sources)
{
  sources.reserve(m_activeSources.size());
  for (auto const& source : m_activeSources) {
    sources.push_back(source->ID());
  }
}

void
RequestManager::addConnections(cms::Exception &ex)
{
  std::vector<std::string> sources;
  getActiveSourceNames(sources);
  for (auto const& source : sources)
  {
    ex.addAdditionalInfo("Active source: " + source);
  }
}

IOSize
handle(void * into, IOSize, IOOffset off)
{

  ClientRequest c(into, off, size, r);
  // TODO XXX: guard access to sources with a lock.
  /*
  with (lock) {
  if (m_activeSources.size() == 2)
  {
      m_activeSources[nextInitialSourceToggle]->handle(c);
      if (nextInitialSourceToggle) nextInitialSourceToggle = false;
      else nextInitialSourceToggle = true;
  }
  }
      c.fulfill();
   */
  m_activeSources[0]->handle(c);
  c.fulfill();

  IOSize bytesRead;
  std::shared_ptr<XRootDStatus> status = c.getStatus(bytesRead);

  // TODO: implement recovery mechanisms.
  if (!status.IsOK())
  {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdRequestManager::handle(name='" << m_name
       << "', offset=" << off << ", n=" << size
       << ") failed with error '" << status.ToString()
       << "' (errno=" << status.errNo << ", code="
       << status.code << ")";
    ex.addContext("Calling XrdRequestManager::handle()");
    addConnections(ex);
    throw ex;
  }
  return bytesRead;
}

IOSize
handle(std::vector<std::shared_ptr<IOPosBuffer> > iolist)
{
  ClientRequest c(iolist);
  m_activeSources[0]->handle(c);
  c.fulfill();

  auto status = c.getStatus();
  if (!status.IsOK())
  {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdRequestManager::handle(name='" << m_name
       << ") failed with error '" << status.ToString()
       << "' (errno=" << status.errNo << ", code="
       << status.code << ")";
    ex.addContext("Calling XrdRequestManager::handle()");
    addConnections(ex);
  }
  return 0;
}

std::string
prepareOpaqueString()
{
    // TODO: with source read lock.
    {
    std::stringstream ss;
    ss << "?tried="
    for ( auto & const it : m_activeSources )
        ss << it << ",";
    for ( auto & const it : m_inactiveSources )
        ss << it << ",";
    for ( auto & const it : m_disabledSources )
        ss << it << ",";
    }
    return ss.str();
}

