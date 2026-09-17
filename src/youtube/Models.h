#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace yt {

struct VideoItem {
    std::string id;
    std::string title;
    std::string channelTitle;
    std::string channelId;
    std::string duration{"0:00"};
    uint64_t viewCount{0};
    std::string publishedAt;
    std::string description;
    std::string colorHex{"#2c3e50"};
};

struct ChannelItem {
    std::string channelId;
    std::string channelTitle;
    std::string subscriberCount;
    std::vector<VideoItem> recentVideos;
};

struct VideoDetails {
    std::string id;
    std::string title;
    std::string channelTitle;
    std::string channelId;
    int durationSeconds{0};
    std::string durationFormatted{"0:00"};
    uint64_t viewCount{0};
    uint64_t likeCount{0};
    std::string publishedAt;
    std::string description;
    std::vector<std::string> resolutions;
    std::string streamUri;
};

struct VideoList {
    std::vector<VideoItem> items;
    std::string nextPageToken;
    int totalResults{0};
};

struct SearchResult {
    std::string query;
    std::vector<VideoItem> items;
    std::string nextPageToken;
    int totalResults{0};
};

} // namespace yt
