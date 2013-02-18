
#include <assert.h>
#include <iostream>
#include <algorithm>

#include "XrdCl/XrdClFile.hh"

#include "FWCore/Utilities/interface/CPUTimer.h"
#include "FWCore/Utilities/interface/EDMException.h"

#include "Utilities/XrdAdaptor/src/XrdRequestManager.h"

#define XRD_CL_MAX_CHUNK 512*1024

using namespace XrdAdaptor;

int timeDiffMS(timespec &a, timespec &b)
{
  int diff = (a.tv_sec - b.tv_sec) * 1000;
  diff += (a.tv_nsec - b.tv_nsec) / 1e6;
  return diff;
}

RequestManager::RequestManager(const std::string &filename, XrdCl::OpenFlags::Flags flags, XrdCl::Access::Mode perms)
    : m_file_opening(nullptr),
      m_nextInitialSourceToggle(false),
      m_name(filename),
      m_flags(flags),
      m_perms(perms)
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
  {
    std::lock_guard<std::mutex> sentry(m_source_mutex);
    m_activeSources.push_back(source);
  }

  m_lastSourceCheck = ts;
  ts.tv_sec += 5;
  m_nextActiveSourceCheck = ts;
}

RequestManager::~RequestManager()
{}

void
RequestManager::checkSources(timespec &now, IOSize requestSize)
{
  //std::cout << "Time since last check " << timeDiffMS(now, m_lastSourceCheck) << "; last check " << m_lastSourceCheck.tv_sec << "; now " <<now.tv_sec << std::endl;  
  if (timeDiffMS(now, m_lastSourceCheck) > 1000 && timeDiffMS(now, m_nextActiveSourceCheck) > 0)
  {   
    m_lastSourceCheck = now;
    checkSourcesImpl(now, requestSize);
  }
}

void
RequestManager::checkSourcesImpl(timespec &now, IOSize requestSize)
{
  std::lock_guard<std::mutex> sentry(m_source_mutex);

  bool findNewSource = false;
  if (m_activeSources.size() == 1)
    findNewSource = true;
  else if (m_activeSources.size() > 1)
  {
    if ((m_activeSources[0]->getQuality() > 5130) ||
        ((m_activeSources[0]->getQuality() > 260) && (m_activeSources[0]->getQuality()*4 < m_activeSources[1]->getQuality())))
    {
      m_inactiveSources.emplace_back(std::move(m_activeSources[0]));
      m_activeSources.erase(m_activeSources.begin());
      findNewSource = true;
    }
    else if ((m_activeSources[1]->getQuality() > 5130) ||
        ((m_activeSources[1]->getQuality() > 260) && (m_activeSources[1]->getQuality()*4 < m_activeSources[0]->getQuality())))
    {
      m_inactiveSources.emplace_back(std::move(m_activeSources[1]));
      m_activeSources.erase(m_activeSources.begin()+1);
      findNewSource = true;
    }
  }
  if (findNewSource && !m_file_opening.get())
  {
    auto opaque = prepareOpaqueString();
    std::string new_name = m_name + opaque;
    std::cout << "Trying to open URL: " << new_name << std::endl;
    m_file_opening.reset(new XrdCl::File());
    XrdCl::XRootDStatus status;
    if (!(status = m_file_opening->Open(new_name, m_flags, m_perms, this)).IsOK())
    {
      edm::Exception ex(edm::errors::FileOpenError);
      ex << "XrdCl::File::Open(name='" << new_name
         << "', flags=0x" << std::hex << m_flags
         << ", permissions=0" << std::oct << m_perms << std::dec
         << ") => error '" << status.ToString()
         << "' (errno=" << status.errNo << ", code=" << status.code << ")";
      ex.addContext("Calling XrdFile::open()");
      addConnections(ex);
      throw ex;
    }
    m_lastSourceCheck = now;

  }

  now.tv_sec += 5;
  m_nextActiveSourceCheck = now;
}

std::shared_ptr<XrdCl::File>
RequestManager::getActiveFile()
{
  std::lock_guard<std::mutex> sentry(m_source_mutex);
  return m_activeSources[0]->getFileHandle();
}

void
RequestManager::getActiveSourceNames(std::vector<std::string> & sources)
{
  std::lock_guard<std::mutex> sentry(m_source_mutex);
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

std::future<IOSize>
RequestManager::handle(std::shared_ptr<XrdAdaptor::ClientRequest> c_ptr)
{
  assert(c_ptr.get());
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  checkSources(now, c_ptr->getSize());

  std::shared_ptr<Source> source = nullptr;
  {
    std::lock_guard<std::mutex> sentry(m_source_mutex);
    if (m_activeSources.size() == 2)
    {
        if (m_nextInitialSourceToggle)
        {
            source = m_activeSources[0];
            m_nextInitialSourceToggle = false;
        }
        else
        {
            source = m_activeSources[1];
            m_nextInitialSourceToggle = true;
        }
    }
    else
    {
        source = m_activeSources[0];
    }
  }
  source->handle(c_ptr);
  return c_ptr->get_future();
}

std::string
RequestManager::prepareOpaqueString()
{
    // TODO: with source read lock.
    {
    std::stringstream ss;
    ss << "?tried=";
    size_t count = 0;
    for ( const auto & it : m_activeSources )
    {
        count++;
        ss << it->ID().substr(0, it->ID().find(":")) << ",";
    }
    for ( const auto & it : m_inactiveSources )
    {
        count++;
        ss << it->ID().substr(0, it->ID().find(":")) << ",";
    }
    for ( const auto & it : m_disabledSources )
    {
        count++;
        ss << it.substr(0, it.find(":")) << ",";
    }
    if (count)
    {
        std::string tmp_str = ss.str();
        return tmp_str.substr(0, tmp_str.size()-1);
    }
    return ss.str();
    }
}

void 
XrdAdaptor::RequestManager::HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList)
{
    //std::cout << "Open callback" << std::endl;
    if (status->IsOK())
    {
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        std::shared_ptr<Source> source(new Source(now, std::move(m_file_opening)));
        std::cout << "successfully opened new source: " << source->ID() << std::endl;
        { 
            std::lock_guard<std::mutex> sentry(m_source_mutex);
            m_activeSources.push_back(source);
        }
    }
    else
    {   // File-open failure - wait at least 60s before next attempt.
        m_nextActiveSourceCheck.tv_sec += 2*60;
    }
    delete status;
    delete hostList;
}

std::future<IOSize>
XrdAdaptor::RequestManager::handle(std::shared_ptr<std::vector<IOPosBuffer> > iolist)
{
    std::lock_guard<std::mutex> sentry(m_source_mutex);

    edm::CPUTimer timer;
    timer.start();

    if (m_activeSources.size() == 1)
    {
        std::shared_ptr<XrdAdaptor::ClientRequest> c_ptr(new XrdAdaptor::ClientRequest(*this, iolist));
        m_activeSources[0]->handle(c_ptr);
        return c_ptr->get_future();
    }

    assert(iolist.get());
    std::shared_ptr<std::vector<IOPosBuffer> > req1(new std::vector<IOPosBuffer>);
    std::shared_ptr<std::vector<IOPosBuffer> > req2(new std::vector<IOPosBuffer>);
    splitClientRequest(*iolist, *req1, *req2);

    std::shared_ptr<XrdAdaptor::ClientRequest> c_ptr1, c_ptr2;
    std::future<IOSize> future1, future2;
    if (req1->size())
    {
        c_ptr1.reset(new XrdAdaptor::ClientRequest(*this, req1));
        m_activeSources[0]->handle(c_ptr1);
        future1 = c_ptr1->get_future();
    }
    if (req2->size())
    {
        c_ptr2.reset(new XrdAdaptor::ClientRequest(*this, req2));
        m_activeSources[1]->handle(c_ptr2);
        future2 = c_ptr2->get_future();
    }
    if (req1->size() && req2->size())
    {
        std::future<IOSize> task = std::async(std::launch::deferred,
            [](std::future<IOSize> a, std::future<IOSize> b){
                return b.get() + a.get();
            },
            std::move(future1),
            std::move(future2));
        timer.stop();
        std::cout << "Total time to create requests " << static_cast<int>(1000*timer.realTime()) << std::endl;
        return task;
    }
    if (req1->size()) return future1;
    if (req2->size()) return future2;

    std::promise<IOSize> p; p.set_value(0);
    return p.get_future();
}

static void
consumeChunkFront(size_t &front, std::vector<IOPosBuffer> &input, std::vector<IOPosBuffer> &output, IOSize chunksize)
{
    while ((chunksize > 0) && (front < input.size()))
    {
        IOPosBuffer &io = input[front];
        if (io.size() > chunksize)
        {
            IOSize newsize = io.size() - chunksize;
            IOOffset newoffset = io.offset() + chunksize;
            void* newdata = static_cast<char*>(io.data()) + chunksize;
            output.emplace_back(IOPosBuffer(io.offset(), io.data(), chunksize));
            io.set_offset(newoffset);
            io.set_data(newdata);
            io.set_size(newsize);
            chunksize = 0;
        }
        else
        {
            output.push_back(io);
            chunksize -= io.size();
            front++;
        }
    }
}

static void
consumeChunkBack(size_t front, std::vector<IOPosBuffer> &input, std::vector<IOPosBuffer> &output, IOSize chunksize)
{
    while ((chunksize > 0) && (front < input.size()))
    {
        IOPosBuffer &io = input.back();
        if (io.size() > chunksize)
        {
            IOSize newsize = io.size() - chunksize;
            IOOffset newoffset = io.offset() + chunksize;
            void* newdata = static_cast<char*>(io.data()) + chunksize;
            output.emplace_back(IOPosBuffer(io.offset(), io.data(), chunksize));
            io.set_offset(newoffset);
            io.set_data(newdata);
            io.set_size(newsize);
            chunksize = 0;
        }
        else
        {
            output.push_back(io);
            chunksize -= io.size();
            input.pop_back();
        }
    }
}

void
XrdAdaptor::RequestManager::splitClientRequest(const std::vector<IOPosBuffer> &iolist, std::vector<IOPosBuffer> &req1, std::vector<IOPosBuffer> &req2)
{
    if (iolist.size() == 0) return;
    std::vector<IOPosBuffer> tmp_iolist(iolist.begin(), iolist.end());
    req1.reserve(iolist.size()/2+1);
    req2.reserve(iolist.size()/2+1);
    size_t front=0;

    float q1 = static_cast<float>(m_activeSources[0]->getQuality());
    float q2 = static_cast<float>(m_activeSources[1]->getQuality());
    IOSize chunk1, chunk2;
    chunk1 = static_cast<float>(XRD_CL_MAX_CHUNK)*(q2/(q1+q2));
    chunk2 = static_cast<float>(XRD_CL_MAX_CHUNK)*(q1/(q1+q2));

    while (tmp_iolist.size()-front > 0)
    {
        consumeChunkFront(front, tmp_iolist, req1, chunk1);
        consumeChunkBack(front, tmp_iolist, req2, chunk2);
    }

    IOSize size1 = 0, size2 = 0, size_orig = 0;
    for (const auto & it : iolist) size_orig += it.size();
    for (const auto & it : req1) size1 += it.size();
    for (const auto & it : req2) size2 += it.size();

    assert(size_orig == size1 + size2);

    std::cout << "Original request size " << iolist.size() << "(" << size_orig << " bytes) split into requests size " << req1.size() << " (" << size1 << " bytes) and " << req2.size() << " (" << size2 << " bytes)" << std::endl;
}

