
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

RequestManager::RequestManager(const std::string &filename, int flags, int perms)
{
  m_name = filename;
  m_flags = flags;


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

  std::shared_ptr<Source> source(new Source(std::move(file)));
  m_activeSources.push_back(source);
}

RequestManager::~RequestManager()
{}

void
RequestManager::checkSources(timespec &now, IOSize requestSize)
{   
  if (timeDiffMS(now, lastSourceCheck) > 1000)
  {   
    lastSourceCheck = now;
    checkSourcesImpl(now, requestSize);
  }
}

void
RequestManager::getActiveSourceNames(std::vector<std::string> & sources)
{
  sources.reserve(m_activeSources.size());
  for (auto source : m_activeSources) {
    sources.push_back(source->ID());
  }
}
