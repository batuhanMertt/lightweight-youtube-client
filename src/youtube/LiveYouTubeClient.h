#pragma once

#include "youtube/IYouTubeClient.h"
#include "network/IHttpClient.h"
#include <memory>
#include <string>

namespace yt {

class LiveYouTubeClient : public IYouTubeClient {
public:
    explicit LiveYouTubeClient(std::shared_ptr<IHttpClient> httpClient = nullptr, const std::string& apiKey = "");

    Result<SearchResult> search(const std::string& query, const std::string& pageToken = "") override;
    Result<VideoList> getHomeFeed(int maxResults = 20) override;
    Result<std::vector<ChannelItem>> getSubscriptions(int maxChannels = 10) override;
    Result<VideoDetails> getVideoInfo(const std::string& videoId) override;

    void setApiKey(const std::string& apiKey) { m_apiKey = apiKey; }
    void setAccessToken(const std::string& token) { m_accessToken = token; }

private:
    std::vector<VideoItem> extractVideosFromYouTubeSearch(const std::string& query, int maxResults);
    bool fetchOEmbedMetadata(const std::string& videoId, VideoItem& outItem);

    std::shared_ptr<IHttpClient> m_http;
    std::string m_apiKey;
    std::string m_accessToken;
};

} // namespace yt
