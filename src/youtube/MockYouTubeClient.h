#pragma once

#include "youtube/IYouTubeClient.h"
#include <string>

namespace yt {

class MockYouTubeClient : public IYouTubeClient {
public:
    explicit MockYouTubeClient(const std::string& assetsPath = "assets");

    Result<SearchResult> search(const std::string& query, const std::string& pageToken = "") override;
    Result<VideoList> getHomeFeed(int maxResults = 20) override;
    Result<std::vector<ChannelItem>> getSubscriptions(int maxChannels = 10) override;
    Result<VideoDetails> getVideoInfo(const std::string& videoId) override;

private:
    std::string m_assetsPath;
    std::string readFileContent(const std::string& relativePath) const;
};

} // namespace yt
