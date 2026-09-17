#include "youtube/YouTubeClient.h"
#include "youtube/JsonParser.h"
#include "network/HttpClientFactory.h"
#include "core/Logger.h"
#include <iomanip>
#include <sstream>
#include <cctype>

namespace yt {

YouTubeClient::YouTubeClient(std::shared_ptr<IHttpClient> httpClient, const std::string& apiKey)
    : m_http(httpClient), m_apiKey(apiKey) {
    if (!m_http) {
        m_http = HttpClientFactory::create();
    }
    LOG_INFO("Live YouTube API Client initialized (configured for YouTube Data API v3)");
}

void YouTubeClient::setApiKey(const std::string& apiKey) {
    m_apiKey = apiKey;
    LOG_INFO("YouTube API key configured");
}

void YouTubeClient::setAccessToken(const std::string& token) {
    m_accessToken = token;
    LOG_INFO("YouTube Access Token updated");
}

std::string YouTubeClient::urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

std::string YouTubeClient::parseIso8601Duration(const std::string& iso) {
    // Format: PT#H#M#S or PT#M#S or PT#S
    if (iso.empty() || iso[0] != 'P') return "0:00";

    int hours = 0, minutes = 0, seconds = 0;
    int currentNum = 0;

    for (size_t i = 1; i < iso.length(); ++i) {
        char c = iso[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            currentNum = currentNum * 10 + (c - '0');
        } else if (c == 'H') {
            hours = currentNum;
            currentNum = 0;
        } else if (c == 'M') {
            minutes = currentNum;
            currentNum = 0;
        } else if (c == 'S') {
            seconds = currentNum;
            currentNum = 0;
        }
    }

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << ":" << std::setfill('0') << std::setw(2) << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    } else {
        oss << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    }
    return oss.str();
}

Result<VideoList> YouTubeClient::getHomeFeed(int maxResults) {
    if (!m_http) {
        return Result<VideoList>::Fail(ErrorCode::NETWORK_UNAVAILABLE, "HTTP client not available");
    }
    if (m_apiKey.empty()) {
        return Result<VideoList>::Fail(ErrorCode::API_ERROR, "YouTube API key not configured");
    }

    std::string url = "https://www.googleapis.com/youtube/v3/videos?part=snippet,contentDetails,statistics"
                      "&chart=mostPopular"
                      "&maxResults=" + std::to_string(maxResults) +
                      "&key=" + m_apiKey;

    LOG_INFO("Fetching YouTube Home Feed from API");
    HttpResponse resp = m_http->get(url, "", m_timeoutMs);
    if (!resp.isSuccess()) {
        LOG_ERROR("Home feed API request failed: " + resp.errorMessage);
        return Result<VideoList>::Fail(resp.error, resp.errorMessage);
    }

    JsonValue root;
    if (!JsonParser::parse(resp.body, root)) {
        return Result<VideoList>::Fail(ErrorCode::JSON_PARSE_ERROR, "Failed to parse API response");
    }

    VideoList list;
    list.nextPageToken = root.getString("nextPageToken", "");
    const auto& items = root["items"].asArray();

    for (const auto& itemVal : items) {
        VideoItem item;
        item.id = itemVal.getString("id");

        const auto& snip = itemVal["snippet"];
        item.title = snip.getString("title");
        item.channelTitle = snip.getString("channelTitle");
        item.channelId = snip.getString("channelId");
        item.publishedAt = snip.getString("publishedAt");
        item.description = snip.getString("description");

        const auto& details = itemVal["contentDetails"];
        std::string isoDur = details.getString("duration");
        item.duration = parseIso8601Duration(isoDur);

        const auto& stats = itemVal["statistics"];
        item.viewCount = stats.getUInt64("viewCount", 0);
        item.colorHex = "#2c3e50";

        list.items.push_back(item);
    }

    list.totalResults = static_cast<int>(list.items.size());
    LOG_INFO("Home feed fetched: " + std::to_string(list.totalResults) + " videos");
    return Result<VideoList>::Ok(list);
}

Result<SearchResult> YouTubeClient::search(const std::string& query, const std::string& pageToken) {
    if (!m_http) {
        return Result<SearchResult>::Fail(ErrorCode::NETWORK_UNAVAILABLE, "HTTP client not available");
    }
    if (m_apiKey.empty()) {
        return Result<SearchResult>::Fail(ErrorCode::API_ERROR, "YouTube API key not configured");
    }

    std::string encQ = urlEncode(query);
    std::string url = "https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=10"
                      "&q=" + encQ +
                      "&key=" + m_apiKey;
    if (!pageToken.empty()) {
        url += "&pageToken=" + pageToken;
    }

    LOG_INFO("Performing live YouTube search for: " + query);
    HttpResponse resp = m_http->get(url, "", m_timeoutMs);
    if (!resp.isSuccess()) {
        LOG_ERROR("Search API request failed: " + resp.errorMessage);
        return Result<SearchResult>::Fail(resp.error, resp.errorMessage);
    }

    JsonValue root;
    if (!JsonParser::parse(resp.body, root)) {
        return Result<SearchResult>::Fail(ErrorCode::JSON_PARSE_ERROR);
    }

    SearchResult res;
    res.query = query;
    res.nextPageToken = root.getString("nextPageToken", "");
    const auto& items = root["items"].asArray();

    for (const auto& itemVal : items) {
        VideoItem item;
        item.id = itemVal["id"].getString("videoId");

        const auto& snip = itemVal["snippet"];
        item.title = snip.getString("title");
        item.channelTitle = snip.getString("channelTitle");
        item.channelId = snip.getString("channelId");
        item.publishedAt = snip.getString("publishedAt");
        item.description = snip.getString("description");
        item.duration = "LIVE"; // Search snippet doesn't include duration
        item.colorHex = "#2980b9";

        res.items.push_back(item);
    }

    res.totalResults = static_cast<int>(res.items.size());
    LOG_INFO("Search returned " + std::to_string(res.totalResults) + " results");
    return Result<SearchResult>::Ok(res);
}

Result<std::vector<ChannelItem>> YouTubeClient::getSubscriptions(int maxChannels) {
    if (!m_http) {
        return Result<std::vector<ChannelItem>>::Fail(ErrorCode::NETWORK_UNAVAILABLE);
    }
    if (m_accessToken.empty()) {
        return Result<std::vector<ChannelItem>>::Fail(ErrorCode::AUTHENTICATION_EXPIRED, "Authentication required for subscriptions");
    }

    std::string url = "https://www.googleapis.com/youtube/v3/subscriptions?part=snippet&mine=true"
                      "&maxResults=" + std::to_string(maxChannels);

    LOG_INFO("Fetching user subscriptions from YouTube API");
    HttpResponse resp = m_http->get(url, m_accessToken, m_timeoutMs);
    if (!resp.isSuccess()) {
        return Result<std::vector<ChannelItem>>::Fail(resp.error, resp.errorMessage);
    }

    JsonValue root;
    if (!JsonParser::parse(resp.body, root)) {
        return Result<std::vector<ChannelItem>>::Fail(ErrorCode::JSON_PARSE_ERROR);
    }

    std::vector<ChannelItem> channels;
    const auto& items = root["items"].asArray();

    for (const auto& itemVal : items) {
        ChannelItem ch;
        const auto& snip = itemVal["snippet"];
        ch.channelId = snip["resourceId"].getString("channelId");
        ch.channelTitle = snip.getString("title");
        ch.subscriberCount = "Subscribed";
        channels.push_back(ch);
    }

    return Result<std::vector<ChannelItem>>::Ok(channels);
}

Result<VideoDetails> YouTubeClient::getVideoInfo(const std::string& videoId) {
    if (!m_http) {
        return Result<VideoDetails>::Fail(ErrorCode::NETWORK_UNAVAILABLE);
    }
    if (m_apiKey.empty()) {
        return Result<VideoDetails>::Fail(ErrorCode::API_ERROR, "API key not configured");
    }

    std::string url = "https://www.googleapis.com/youtube/v3/videos?part=snippet,contentDetails,statistics"
                      "&id=" + videoId +
                      "&key=" + m_apiKey;

    HttpResponse resp = m_http->get(url, "", m_timeoutMs);
    if (!resp.isSuccess()) {
        return Result<VideoDetails>::Fail(resp.error, resp.errorMessage);
    }

    JsonValue root;
    if (!JsonParser::parse(resp.body, root)) {
        return Result<VideoDetails>::Fail(ErrorCode::JSON_PARSE_ERROR);
    }

    const auto& items = root["items"].asArray();
    if (items.empty()) {
        return Result<VideoDetails>::Fail(ErrorCode::NO_SEARCH_RESULT, "Video not found");
    }

    const auto& first = items[0];
    VideoDetails details;
    details.id = videoId;

    const auto& snip = first["snippet"];
    details.title = snip.getString("title");
    details.channelTitle = snip.getString("channelTitle");
    details.channelId = snip.getString("channelId");
    details.publishedAt = snip.getString("publishedAt");
    details.description = snip.getString("description");

    const auto& stats = first["statistics"];
    details.viewCount = stats.getUInt64("viewCount");
    details.likeCount = stats.getUInt64("likeCount");

    const auto& detailsNode = first["contentDetails"];
    details.durationFormatted = parseIso8601Duration(detailsNode.getString("duration"));
    details.streamUri = "https://www.youtube.com/watch?v=" + videoId;

    return Result<VideoDetails>::Ok(details);
}

} // namespace yt
