#ifndef Utilities_XrdAdaptor_XrdRequestManager_h
#define Utilities_XrdAdaptor_XrdRequestManager_h

#include <boost/utility.hpp>

#include "XrdRequest.h"

namespace XrdAdaptor {

class RequestManager : boost::noncopyable {

public:
    RequestManager(const std::string & filename);

    /**
     * Synchronous interface for handling a client request.
     */
    IOSize handle(IOOffset off, IOSize size);
    IOSize handle(ClientRequest);

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
    void broadcastRequest(const Request &, bool active)

    /**
     * Check our set of active sources.
     * If necessary, this will kick off a search for a new source.
     * The source check is somewhat expensive so it is only done once every
     * second.
     */
    void checkSources(timespec &now, IOSize requestSize)
    {
        if (timeDiffMS(now, lastSourceCheck) > 1000)
        {
            lastSourceCheck = now;
            checkSourcesImpl(now, requestSize);
        }
    }
    void checkSourcesImpl(timespec &now, IOSize requestSize);

    void sourceOpenCallback();
    std::string prepareOpaqueString();

    std::vector activeSources;
    std::vector inactiveSources;
    std::vector disabledSources;
    timespec lastSourceCheck;
    bool nextInitialSourceToggle;
    timespec nextActiveSourceCheck;
    bool searchMode;
};

}

#endif
