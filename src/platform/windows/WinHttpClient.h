#pragma once

#include "network/IHttpClient.h"
#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>

namespace yt {

class WinHttpClient : public IHttpClient {
public:
    WinHttpClient();
    ~WinHttpClient() override;

    HttpResponse send(const HttpRequest& req) override;
    HttpResponse get(const std::string& url, const std::string& bearerToken = "", int timeoutMs = 5000) override;
    HttpResponse post(const std::string& url, const std::string& body, const std::string& contentType = "application/json", int timeoutMs = 5000) override;

private:
    HINTERNET m_hSession{nullptr};
};

} // namespace yt
#endif
