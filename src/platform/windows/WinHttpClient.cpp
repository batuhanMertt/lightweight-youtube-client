#include "platform/windows/WinHttpClient.h"
#if defined(_WIN32)
#include "core/Logger.h"
#include <vector>
#include <sstream>

namespace yt {

static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    if (!wstr.empty() && wstr.back() == L'\0') {
        wstr.pop_back();
    }
    return wstr;
}

WinHttpClient::WinHttpClient() {
    m_hSession = WinHttpOpen(
        L"LightweightYouTubeClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!m_hSession) {
        LOG_ERROR("Failed to initialize WinHttp session");
    }
}

WinHttpClient::~WinHttpClient() {
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
    }
}

HttpResponse WinHttpClient::send(const HttpRequest& req) {
    HttpResponse resp;
    if (!m_hSession) {
        resp.error = ErrorCode::NETWORK_UNAVAILABLE;
        resp.errorMessage = "WinHttp session not available";
        return resp;
    }

    std::wstring wUrl = utf8ToWide(req.url);

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.length()), 0, &urlComp)) {
        resp.error = ErrorCode::HTTP_ERROR;
        resp.errorMessage = "Failed to parse URL: " + req.url;
        return resp;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);
    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hConnect = WinHttpConnect(m_hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        resp.error = ErrorCode::NETWORK_UNAVAILABLE;
        resp.errorMessage = "Failed to connect to host: " + std::string(hostName.begin(), hostName.end());
        return resp;
    }

    std::wstring wMethod = utf8ToWide(req.method);
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        wMethod.c_str(),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        resp.error = ErrorCode::HTTP_ERROR;
        resp.errorMessage = "Failed to create HTTP request";
        return resp;
    }

    // Set Timeouts
    WinHttpSetTimeouts(hRequest, req.timeoutMs, req.timeoutMs, req.timeoutMs, req.timeoutMs);

    // Build headers
    std::wstring wHeaders;
    for (const auto& h : req.headers) {
        wHeaders += utf8ToWide(h.first) + L": " + utf8ToWide(h.second) + L"\r\n";
    }

    BOOL sent = WinHttpSendRequest(
        hRequest,
        wHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wHeaders.c_str(),
        static_cast<DWORD>(wHeaders.length()),
        req.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)req.body.c_str(),
        static_cast<DWORD>(req.body.length()),
        static_cast<DWORD>(req.body.length()),
        0
    );

    if (!sent) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        if (err == ERROR_WINHTTP_TIMEOUT) {
            resp.error = ErrorCode::NETWORK_TIMEOUT;
            resp.errorMessage = "Request timed out after " + std::to_string(req.timeoutMs) + "ms";
        } else {
            resp.error = ErrorCode::NETWORK_UNAVAILABLE;
            resp.errorMessage = "Network request failed (code: " + std::to_string(err) + ")";
        }
        return resp;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        resp.error = (err == ERROR_WINHTTP_TIMEOUT) ? ErrorCode::NETWORK_TIMEOUT : ErrorCode::HTTP_ERROR;
        resp.errorMessage = "Failed to receive response";
        return resp;
    }

    // Status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );
    resp.statusCode = static_cast<int>(statusCode);

    // Read Response Body
    DWORD bytesAvailable = 0;
    std::string responseBody;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            responseBody.append(buffer.data(), bytesRead);
        } else {
            break;
        }
    }

    resp.body = responseBody;
    resp.error = (resp.statusCode >= 200 && resp.statusCode < 300) ? ErrorCode::OK : ErrorCode::HTTP_ERROR;

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return resp;
}

HttpResponse WinHttpClient::get(const std::string& url, const std::string& bearerToken, int timeoutMs) {
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.timeoutMs = timeoutMs;
    if (!bearerToken.empty()) {
        req.headers["Authorization"] = "Bearer " + bearerToken;
    }
    return send(req);
}

HttpResponse WinHttpClient::post(const std::string& url, const std::string& body, const std::string& contentType, int timeoutMs) {
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.body = body;
    req.timeoutMs = timeoutMs;
    req.headers["Content-Type"] = contentType;
    return send(req);
}

} // namespace yt
#endif
