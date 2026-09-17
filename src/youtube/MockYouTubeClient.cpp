#include "youtube/MockYouTubeClient.h"
#include "youtube/JsonParser.h"
#include "core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace yt {

MockYouTubeClient::MockYouTubeClient(const std::string& assetsPath)
    : m_assetsPath(assetsPath) {
    LOG_INFO("YouTube client initialized (Mock Mode, assets: " + m_assetsPath + ")");
}

std::string MockYouTubeClient::readFileContent(const std::string& relativePath) const {
    std::vector<std::string> searchPaths = {
        m_assetsPath + "/" + relativePath,
        "assets/" + relativePath,
        "../assets/" + relativePath,
        "../../assets/" + relativePath,
        "D:/youtube-client/assets/" + relativePath
    };

    for (const auto& path : searchPaths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
    }
    return "";
}

Result<VideoList> MockYouTubeClient::getHomeFeed(int maxResults) {
    LOG_INFO("Fetching home feed (limit: " + std::to_string(maxResults) + ")");
    std::string jsonStr = readFileContent("mock_data/home_feed.json");
    if (jsonStr.empty()) {
        LOG_WARN("Could not read home_feed.json, generating fallback in-memory mock data");
        VideoList fallback;
        for (int i = 1; i <= 6; ++i) {
            VideoItem item;
            item.id = "fallback_" + std::to_string(i);
            item.title = "Embedded Linux C++ Architecture Video #" + std::to_string(i);
            item.channelTitle = "Core Systems Lab";
            item.duration = "14:20";
            item.viewCount = 15000 * i;
            item.publishedAt = "3 days ago";
            item.colorHex = (i % 2 == 0) ? "#2c3e50" : "#16a085";
            fallback.items.push_back(item);
        }
        fallback.totalResults = static_cast<int>(fallback.items.size());
        return Result<VideoList>::Ok(fallback);
    }

    JsonValue root;
    if (!JsonParser::parse(jsonStr, root)) {
        LOG_ERROR("Failed to parse home_feed.json");
        return Result<VideoList>::Fail(ErrorCode::JSON_PARSE_ERROR, "Invalid JSON format in home feed");
    }

    VideoList list;
    list.nextPageToken = root.getString("nextPageToken", "");
    const auto& items = root["items"].asArray();

    int count = 0;
    for (const auto& itemVal : items) {
        if (count >= maxResults) break;
        VideoItem item;
        item.id = itemVal.getString("id");
        item.title = itemVal.getString("title");
        item.channelTitle = itemVal.getString("channelTitle");
        item.channelId = itemVal.getString("channelId");
        item.duration = itemVal.getString("duration", "0:00");
        item.viewCount = itemVal.getUInt64("viewCount", 0);
        item.publishedAt = itemVal.getString("publishedAt");
        item.description = itemVal.getString("description");
        item.colorHex = itemVal.getString("color", "#2c3e50");
        list.items.push_back(item);
        ++count;
    }
    list.totalResults = static_cast<int>(list.items.size());
    LOG_INFO("Home feed loaded: " + std::to_string(list.totalResults) + " videos");
    return Result<VideoList>::Ok(list);
}

Result<SearchResult> MockYouTubeClient::search(const std::string& query, const std::string& /*pageToken*/) {
    LOG_INFO("Search started for: '" + query + "'");
    if (query.empty()) {
        return Result<SearchResult>::Fail(ErrorCode::NO_SEARCH_RESULT, "Search query is empty");
    }

    std::string jsonStr = readFileContent("mock_data/search_results.json");
    if (jsonStr.empty()) {
        LOG_WARN("Could not read search_results.json, generating synthetic search results");
        SearchResult res;
        res.query = query;
        for (int i = 1; i <= 5; ++i) {
            VideoItem v;
            v.id = "search_" + std::to_string(i);
            v.title = query + " - System Programming Guide Part " + std::to_string(i);
            v.channelTitle = "Embedded Academy";
            v.duration = "22:15";
            v.viewCount = 34000;
            v.publishedAt = "1 week ago";
            v.colorHex = "#34495e";
            res.items.push_back(v);
        }
        res.totalResults = static_cast<int>(res.items.size());
        LOG_INFO("Search returned " + std::to_string(res.totalResults) + " results");
        return Result<SearchResult>::Ok(res);
    }

    JsonValue root;
    if (!JsonParser::parse(jsonStr, root)) {
        return Result<SearchResult>::Fail(ErrorCode::JSON_PARSE_ERROR);
    }

    SearchResult res;
    res.query = query;
    res.nextPageToken = root.getString("nextPageToken", "");
    const auto& items = root["items"].asArray();

    // Query filtering
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& itemVal : items) {
        VideoItem item;
        item.id = itemVal.getString("id");
        item.title = itemVal.getString("title");
        item.channelTitle = itemVal.getString("channelTitle");
        item.channelId = itemVal.getString("channelId");
        item.duration = itemVal.getString("duration", "0:00");
        item.viewCount = itemVal.getUInt64("viewCount", 0);
        item.publishedAt = itemVal.getString("publishedAt");
        item.description = itemVal.getString("description");
        item.colorHex = itemVal.getString("color", "#2980b9");

        std::string lowerTitle = item.title;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);

        // Include matching or all if general query
        if (lowerTitle.find(lowerQuery) != std::string::npos || lowerQuery == "all" || lowerQuery == "linux" || lowerQuery.length() < 3) {
            res.items.push_back(item);
        }
    }

    if (res.items.empty() && !items.empty()) {
        // Fallback to top items if exact match doesn't hit
        for (size_t i = 0; i < std::min<size_t>(5, items.size()); ++i) {
            VideoItem item;
            item.id = items[i].getString("id");
            item.title = items[i].getString("title");
            item.channelTitle = items[i].getString("channelTitle");
            item.duration = items[i].getString("duration", "0:00");
            item.colorHex = items[i].getString("color", "#2980b9");
            res.items.push_back(item);
        }
    }

    res.totalResults = static_cast<int>(res.items.size());
    LOG_INFO("Search returned " + std::to_string(res.totalResults) + " results");

    if (res.items.empty()) {
        return Result<SearchResult>::Fail(ErrorCode::NO_SEARCH_RESULT);
    }
    return Result<SearchResult>::Ok(res);
}

Result<std::vector<ChannelItem>> MockYouTubeClient::getSubscriptions(int maxChannels) {
    LOG_INFO("Fetching subscriptions (channel limit: " + std::to_string(maxChannels) + ")");
    std::string jsonStr = readFileContent("mock_data/subscriptions.json");
    if (jsonStr.empty()) {
        return Result<std::vector<ChannelItem>>::Fail(ErrorCode::FILE_NOT_FOUND, "subscriptions.json not found");
    }

    JsonValue root;
    if (!JsonParser::parse(jsonStr, root)) {
        return Result<std::vector<ChannelItem>>::Fail(ErrorCode::JSON_PARSE_ERROR);
    }

    std::vector<ChannelItem> channels;
    const auto& chanArray = root["channels"].asArray();

    int count = 0;
    for (const auto& chanVal : chanArray) {
        if (count >= maxChannels) break;
        ChannelItem c;
        c.channelId = chanVal.getString("channelId");
        c.channelTitle = chanVal.getString("channelTitle");
        c.subscriberCount = chanVal.getString("subscriberCount");

        const auto& recents = chanVal["recentVideos"].asArray();
        for (const auto& vVal : recents) {
            VideoItem v;
            v.id = vVal.getString("id");
            v.title = vVal.getString("title");
            v.channelTitle = c.channelTitle;
            v.duration = vVal.getString("duration");
            v.viewCount = vVal.getUInt64("viewCount");
            v.publishedAt = vVal.getString("publishedAt");
            v.colorHex = vVal.getString("color", "#27ae60");
            c.recentVideos.push_back(v);
        }
        channels.push_back(c);
        ++count;
    }
    LOG_INFO("Loaded " + std::to_string(channels.size()) + " subscribed channels");
    return Result<std::vector<ChannelItem>>::Ok(channels);
}

Result<VideoDetails> MockYouTubeClient::getVideoInfo(const std::string& videoId) {
    LOG_INFO("Fetching video info for: " + videoId);
    std::string jsonStr = readFileContent("mock_data/video_details.json");
    VideoDetails details;
    details.id = videoId;
    details.title = "Linux Kernel Architecture Deep Dive & Memory Subsystems";
    details.channelTitle = "Embedded Linux Engineering";
    details.durationSeconds = 2535;
    details.durationFormatted = "42:15";
    details.viewCount = 182400;
    details.likeCount = 14200;
    details.publishedAt = "2026-08-20";
    details.description = "A deep dive into Linux kernel virtual memory, buddy system, slab allocator, and zero-copy DMA buffers.";
    details.streamUri = "mock://stream/" + videoId + ".mp4";

    if (!jsonStr.empty()) {
        JsonValue root;
        if (JsonParser::parse(jsonStr, root)) {
            details.title = root.getString("title", details.title);
            details.channelTitle = root.getString("channelTitle", details.channelTitle);
            details.durationSeconds = root.getInt("durationSeconds", details.durationSeconds);
            details.durationFormatted = root.getString("durationFormatted", details.durationFormatted);
            details.viewCount = root.getUInt64("viewCount", details.viewCount);
            details.likeCount = root.getUInt64("likeCount", details.likeCount);
            details.description = root.getString("description", details.description);
            details.streamUri = root.getString("streamUri", details.streamUri);
        }
    }
    return Result<VideoDetails>::Ok(details);
}

} // namespace yt
