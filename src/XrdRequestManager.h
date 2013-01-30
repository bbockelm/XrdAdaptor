#ifndef Utilities_XrdAdaptor_XrdRequestManager_h
#define Utilities_XrdAdaptor_XrdRequestManager_h

#include <boost/utility.hpp>

#include "XrdRequest.h"
#include "XrdSource.h"

namespace XrdCl {
    class File;
}

namespace XrdAdaptor {

class RequestManager : boost::noncopyable {

public:
    RequestManager(const std::string & filename, int flags, int perms);

    ~RequestManager();

    /**
     * Synchronous interface for handling a client request.
     */
    IOSize handle(IOOffset off, IOSize size);
    IOSize handle(ClientRequest);

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

private:
    /**
     * Given a client request, split it into two requests lists.
     */
    void splitClientRequest(const ClientRequest &, RequestList &, RequestList &);

    /**
     * Given a request, broadcast it to all sources.
     * If active is true, broadcast is made to all active sources.
     * Otherwise, broadcast is made to the inactive sources.
     */
    void broadcastRequest(const Request &, bool active);

    /**
     * Check our set of active sources.
     * If necessary, this will kick off a search for a new source.
     * The source check is somewhat expensive so it is only done once every
     * second.
     */
    void checkSources(timespec &now, IOSize requestSize); // TODO: inline
    void checkSourcesImpl(timespec &now, IOSize requestSize);

    /**
     * Invoked when a source (besides the initial source) has finished opening.
     */
    void sourceOpenCallback();

    /**
     * Prepare an opaque string appropriate for asking a redirector to open the
     * current file but avoiding servers which we already have connections to.
     */
    std::string prepareOpaqueString();

    /**
     * Add the list of active connections to the exception extra info.
     */
    void addConnections(cms::Exception &);

    std::vector<std::shared_ptr<Source> > m_activeSources;
    std::vector<std::shared_ptr<Source> > m_inactiveSources;
    std::vector<std::shared_ptr<Source> > m_disabledSources;
    timespec lastSourceCheck;
    bool nextInitialSourceToggle;
    timespec nextActiveSourceCheck;
    bool searchMode;

    std::string m_name;
    int m_flags; // TODO: this was moved to an enum in the client
};

}

#endif
