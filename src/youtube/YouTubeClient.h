#pragma once

#include "youtube/IYouTubeClient.h"
#include "network/IHttpClient.h"
#include <memory>
#include <string>

namespace yt {

class YouTubeClient : public IYouTubeClient {
public:
    explicit YouTubeClient(std::shared_ptr<IHttpClient> httpClient = nullptr, const std::string& apiKey = "");
    ~YouTubeClient() override = default;

    Result<SearchResult> search(const std::string& query, const std::string& pageToken = "") override;
    Result<VideoList> getHomeFeed(int maxResults = 20) override;
    Result<std::vector<ChannelItem>> getSubscriptions(int maxChannels = 10) override;
    Result<VideoDetails> getVideoInfo(const std::string& videoId) override;

    void setApiKey(const std::string& apiKey);
    void setAccessToken(const std::string& token);
    void setTimeoutMs(int timeoutMs) { m_timeoutMs = timeoutMs; }
    int getTimeoutMs() const { return m_timeoutMs; }

    static std::string parseIso8601Duration(const std::string& iso);
    static std::string urlEncode(const std::string& val);

private:
    std::shared_ptr<IHttpClient> m_http;
    std::string m_apiKey;
    std::string m_accessToken;
    int m_timeoutMs{5000};
};

} // namespace yt
