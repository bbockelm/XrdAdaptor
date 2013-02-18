#ifndef Utilities_XrdAdaptor_XrdRequestManager_h
#define Utilities_XrdAdaptor_XrdRequestManager_h

#include <mutex>
#include <sys/stat.h>

#include <boost/utility.hpp>

#include "FWCore/Utilities/interface/EDMException.h"

#include "XrdCl/XrdClFileSystem.hh"

#include "XrdRequest.h"
#include "XrdSource.h"

namespace XrdCl {
    class File;
}

namespace XrdAdaptor {

class RequestManager : boost::noncopyable, public XrdCl::ResponseHandler {

public:
    RequestManager(const std::string & filename, XrdCl::OpenFlags::Flags flags, XrdCl::Access::Mode perms);

    ~RequestManager();

    /**
     * Interface for handling a client request.
     */
    std::future<IOSize> handle(void * into, IOSize size, IOOffset off)
    {
        std::shared_ptr<XrdAdaptor::ClientRequest> c_ptr(new XrdAdaptor::ClientRequest(*this, into, size, off));
        return handle(c_ptr);
    }

    std::future<IOSize> handle(std::shared_ptr<std::vector<IOPosBuffer> > iolist);

    /**
     * Handle a client request.
     * NOTE: The shared_ptr interface is required.  Depending on the state of the manager,
     * it may decide to issue multiple requests and return the first successful.  In that case,
     * some references to the client request may still be outstanding when this function returns.
     */
    std::future<IOSize> handle(std::shared_ptr<XrdAdaptor::ClientRequest> c_ptr);

    /**
     * Retrieve the names of the active sources
     * (primarily meant to enable meaningful log messages).
     */
    void getActiveSourceNames(std::vector<std::string> & sources);

    /**
     * Return a pointer to an active file.  Useful for metadata
     * operations.
     */
    std::shared_ptr<XrdCl::File> getActiveFile();

    /**
     * Add the list of active connections to the exception extra info.
     */
    void addConnections(cms::Exception &);

    /**
     * Return current filename
     */
    const std::string & getFilename() const {return m_name;}

    /**
     * Handle the file-open response
     */
    virtual void HandleResponseWithHosts(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response, XrdCl::HostList *hostList) override;


private:
    /**
     * Given a client request, split it into two requests lists.
     */
    static
    void splitClientRequest(const std::vector<IOPosBuffer> &iolist, std::vector<IOPosBuffer> &req1, std::vector<IOPosBuffer> &req2);

    /**
     * Given a request, broadcast it to all sources.
     * If active is true, broadcast is made to all active sources.
     * Otherwise, broadcast is made to the inactive sources.
     */
    void broadcastRequest(const ClientRequest &, bool active);

    /**
     * Check our set of active sources.
     * If necessary, this will kick off a search for a new source.
     * The source check is somewhat expensive so it is only done once every
     * second.
     */
    void checkSources(timespec &now, IOSize requestSize); // TODO: inline
    void checkSourcesImpl(timespec &now, IOSize requestSize);

    /**
     * Prepare an opaque string appropriate for asking a redirector to open the
     * current file but avoiding servers which we already have connections to.
     */
    std::string prepareOpaqueString();

    std::vector<std::shared_ptr<Source> > m_activeSources;
    std::vector<std::shared_ptr<Source> > m_inactiveSources;
    std::vector<std::string> m_disabledSources;
    std::unique_ptr<XrdCl::File> m_file_opening;
    timespec m_lastSourceCheck;
    // If set to true, the next active source should be 1; 0 otherwise.
    bool m_nextInitialSourceToggle;
    // The time when the next active source check should be performed.
    timespec m_nextActiveSourceCheck;
    bool searchMode;

    const std::string m_name;
    XrdCl::OpenFlags::Flags m_flags;
    XrdCl::Access::Mode m_perms;
    std::mutex m_source_mutex;
};

}

#endif
