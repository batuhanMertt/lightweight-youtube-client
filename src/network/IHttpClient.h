#pragma once

#include "core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace yt {

struct HttpRequest {
    std::string url;
    std::string method{"GET"}; // GET, POST
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    int timeoutMs{5000};
};

struct HttpResponse {
    int statusCode{0};
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    ErrorCode error{ErrorCode::OK};
    std::string errorMessage;

    bool isSuccess() const {
        return statusCode >= 200 && statusCode < 300 && error == ErrorCode::OK;
    }
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    virtual HttpResponse send(const HttpRequest& req) = 0;
    virtual HttpResponse get(const std::string& url, const std::string& bearerToken = "", int timeoutMs = 5000) = 0;
    virtual HttpResponse post(const std::string& url, const std::string& body, const std::string& contentType = "application/json", int timeoutMs = 5000) = 0;
};

} // namespace yt
