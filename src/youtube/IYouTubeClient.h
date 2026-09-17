#pragma once

#include "core/Types.h"
#include "youtube/Models.h"
#include <string>
#include <vector>

namespace yt {

class IYouTubeClient {
public:
    virtual ~IYouTubeClient() = default;

    virtual Result<SearchResult> search(const std::string& query, const std::string& pageToken = "") = 0;
    virtual Result<VideoList> getHomeFeed(int maxResults = 20) = 0;
    virtual Result<std::vector<ChannelItem>> getSubscriptions(int maxChannels = 10) = 0;
    virtual Result<VideoDetails> getVideoInfo(const std::string& videoId) = 0;
};

} // namespace yt
