#ifndef Utilities_XrdAdaptor_XrdRequest_h
#define Utilities_XrdAdaptor_XrdRequest_h

#include "tbb/concurrent_queue.h"

namespace XrdAdaptor {

class Request : boost::noncopyable {

public:
    Request(IOSize, IOOffset, ClientRequest &);
    Request(IOPosBuffer *, ClientRequest &);
    void fulfill(IOSize);
};

class ClientRequest : boost::noncopyable {

public:
    ClientRequest(IOSize, IOOffset);
    ClientRequest(IOPosBuffer *);

    setOutstanding(unsigned);
    void fulfill();
};

class RequestList {

public:
    RequestList(const std::vector<Request> &);
    pop_front();
    pop_back();
    push_front();
    push_back();
};

}

#endif

