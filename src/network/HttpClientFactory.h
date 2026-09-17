#pragma once

#include "network/IHttpClient.h"
#include <memory>

#if defined(_WIN32)
#include "platform/windows/WinHttpClient.h"
#endif

namespace yt {

class HttpClientFactory {
public:
    static std::shared_ptr<IHttpClient> create() {
#if defined(_WIN32)
        return std::make_shared<WinHttpClient>();
#else
        return nullptr; // Linux libcurl backend
#endif
    }
};

} // namespace yt
