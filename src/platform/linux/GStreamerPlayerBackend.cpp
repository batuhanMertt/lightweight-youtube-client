#include "platform/linux/GStreamerPlayerBackend.h"
#include "core/Logger.h"

namespace yt {

GStreamerPlayerBackend::GStreamerPlayerBackend() {
    LOG_INFO("GStreamer backend initialized (ready for ARM/Linux GStreamer 1.0)");
}

GStreamerPlayerBackend::~GStreamerPlayerBackend() {
    stop();
}

std::string GStreamerPlayerBackend::buildHardwarePipeline(const std::string& uri) const {
    // Conceptual embedded pipeline offloading decode to VPU/GPU:
    // souphttpsrc -> qtdemux -> v4l2h264dec (hardware) -> kmssink (zero-copy DRM direct display)
    return "souphttpsrc location=" + uri + " ! qtdemux name=d "
           "d. ! queue max-size-buffers=3 ! v4l2h264dec ! kmssink "
           "d. ! queue max-size-buffers=3 ! aacparse ! alsasink";
}

bool GStreamerPlayerBackend::play(const VideoSource& source) {
    m_source = source;
    m_state = PlaybackState::PLAYING;
    LOG_INFO("GStreamer pipeline launch: " + buildHardwarePipeline(source.streamUri));
    if (m_stateCb) m_stateCb(m_state);
    return true;
}

void GStreamerPlayerBackend::pause() {
    m_state = PlaybackState::PAUSED;
    LOG_INFO("GStreamer pipeline paused");
    if (m_stateCb) m_stateCb(m_state);
}

void GStreamerPlayerBackend::resume() {
    m_state = PlaybackState::PLAYING;
    LOG_INFO("GStreamer pipeline resumed");
    if (m_stateCb) m_stateCb(m_state);
}

void GStreamerPlayerBackend::stop() {
    m_state = PlaybackState::STOPPED;
    m_currentPosition = 0;
    LOG_INFO("GStreamer pipeline stopped");
    if (m_stateCb) m_stateCb(m_state);
}

void GStreamerPlayerBackend::seek(int seconds) {
    m_currentPosition = seconds;
    LOG_INFO("GStreamer pipeline seek: " + std::to_string(seconds) + "s");
    if (m_posCb) m_posCb(m_currentPosition, m_duration);
}

} // namespace yt
