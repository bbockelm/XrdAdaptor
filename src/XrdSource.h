#ifndef Utilities_XrdAdaptor_XrdSource_h
#define Utilities_XrdAdaptor_XrdSource_h

#include <memory>

#include "QualityMetric.h"

namespace XrdCl {
    class File;
}

namespace XrdAdaptor {

class RequestList;

class Source {

public:
    Source(std::unique_ptr<XrdCl::File> fileHandle);

    void handle(RequestList &);

    std::shared_ptr<XrdCl::File> getFileHandle();

    const std::string & ID() const {return m_id;}

    unsigned getQuality() {return m_qm.get();}

private:
    void requestCallback(/* TODO: type? */);

    struct timespec m_lastDowngrade;
    std::string m_id;
    std::unique_ptr<XrdCl::File> m_fh;

    QualityMetric m_qm;

    std::vector<char> m_buffer;
};

}

#endif
