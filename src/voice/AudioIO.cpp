#include "AudioIO.h"

#include <pulse/pulseaudio.h>
#include <rnnoise.h>
#include <modules/audio_processing/include/audio_processing.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr size_t MaxQueuedFrames = 15;  // 300 ms: drop older audio beyond
constexpr size_t PrimeFrames = 2;       // start a participant after 40 ms

pa_proplist* callProperties()
{
    pa_proplist* props = pa_proplist_new();
    pa_proplist_sets(props, PA_PROP_APPLICATION_NAME, "Disports");
    pa_proplist_sets(props, PA_PROP_APPLICATION_ID, "disports.jukfiuu");
    pa_proplist_sets(props, PA_PROP_MEDIA_ROLE, "phone");
    // module-filter-heuristics would add PulseAudio's echo canceller to
    // phone streams, which crashes PulseAudio on Android devices. We do
    // our own.
    pa_proplist_sets(props, "filter.suppress", "echo-cancel");
    return props;
}

void contextStateCallback(pa_context*, void* self) { static_cast<AudioIO*>(self)->onContextState(); }
void readCallback(pa_stream*, size_t bytes, void* self) { static_cast<AudioIO*>(self)->onRead(bytes); }
void writeCallback(pa_stream*, size_t bytes, void* self) { static_cast<AudioIO*>(self)->onWrite(bytes); }
void sinkInfoCallback(pa_context*, const pa_sink_info* info, int eol, void* self)
{
    static_cast<AudioIO*>(self)->onSinkInfo(info, eol);
}
void successCallback(pa_context*, int, void* self)
{
    static_cast<AudioIO*>(self)->onContextState(); // wakes up waitFor()
}
void unref(pa_operation* op)
{
    if (op)
        pa_operation_unref(op);
}

}

AudioIO::AudioIO() = default;

AudioIO::~AudioIO()
{
    stop();
}

void AudioIO::onContextState()
{
    pa_threaded_mainloop_signal(m_loop, 0);
}

bool AudioIO::waitForContext()
{
    for (;;) {
        const pa_context_state_t state = pa_context_get_state(m_context);
        if (state == PA_CONTEXT_READY)
            return true;
        if (!PA_CONTEXT_IS_GOOD(state)) {
            m_error = pa_strerror(pa_context_errno(m_context));
            return false;
        }
        pa_threaded_mainloop_wait(m_loop);
    }
}

bool AudioIO::start(CaptureCallback capture, bool processing, bool denoise)
{
    stop();
    m_capture = std::move(capture);
    m_renderBuffer.clear();
    if (processing)
        setUpProcessing(denoise);
    if (denoise)
        m_denoise = rnnoise_create(nullptr);
    m_loop = pa_threaded_mainloop_new();
    if (!m_loop) {
        m_error = "no PulseAudio main loop";
        return false;
    }

    pa_proplist* props = callProperties();
    m_context = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(m_loop), "Disports", props);
    pa_context_set_state_callback(m_context, &contextStateCallback, this);
    pa_threaded_mainloop_lock(m_loop);
    if (pa_threaded_mainloop_start(m_loop) < 0
            || pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0
            || !waitForContext()) {
        if (m_error.empty())
            m_error = pa_strerror(pa_context_errno(m_context));
        pa_threaded_mainloop_unlock(m_loop);
        pa_proplist_free(props);
        stop();
        return false;
    }

    // Don't switch to the droid "communication" profile: recording while
    // playing in it can break the microphone for every app until a reboot.
    queryPorts();

    // Microphone: 20 ms fragments of mono.
    const pa_sample_spec captureSpec = {PA_SAMPLE_S16LE, SampleRate, 1};
    pa_buffer_attr captureAttr = {};
    captureAttr.maxlength = uint32_t(-1);
    captureAttr.fragsize = FrameSamples * sizeof(int16_t);
    m_record = pa_stream_new_with_proplist(m_context, "Call microphone", &captureSpec, nullptr, props);
    pa_stream_set_read_callback(m_record, &readCallback, this);
    // Timing updates: the echo canceller is told how far behind the
    // microphone the loudspeaker is.
    const auto flags = pa_stream_flags_t(PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE
                                         | PA_STREAM_INTERPOLATE_TIMING);
    pa_stream_connect_record(m_record, nullptr, &captureAttr, flags);

    // Output: stereo, about 60 ms buffered.
    const pa_sample_spec playbackSpec = {PA_SAMPLE_S16LE, SampleRate, 2};
    const uint32_t frameBytes = FrameSamples * 2 * sizeof(int16_t);
    pa_buffer_attr playbackAttr = {};
    playbackAttr.maxlength = uint32_t(-1);
    playbackAttr.tlength = frameBytes * 3;
    playbackAttr.prebuf = frameBytes;
    playbackAttr.minreq = frameBytes;
    m_playback = pa_stream_new_with_proplist(m_context, "Call", &playbackSpec, nullptr, props);
    pa_stream_set_write_callback(m_playback, &writeCallback, this);
    pa_stream_connect_playback(m_playback, nullptr, &playbackAttr, flags, nullptr, nullptr);

    pa_proplist_free(props);
    pa_threaded_mainloop_unlock(m_loop);
    return true;
}

void AudioIO::stop()
{
    if (!m_loop)
        return;
    pa_threaded_mainloop_lock(m_loop);
    // Unhooked first: waiting below lets PulseAudio's thread run callbacks
    // it has already queued for these streams.
    for (pa_stream** stream : {&m_record, &m_playback}) {
        if (*stream) {
            pa_stream_set_read_callback(*stream, nullptr, nullptr);
            pa_stream_set_write_callback(*stream, nullptr, nullptr);
            pa_stream_disconnect(*stream);
            pa_stream_unref(*stream);
            *stream = nullptr;
        }
    }
    // Leave the output as it was before the call.
    if (m_context && !m_sinkName.empty() && !m_originalPort.empty())
        waitFor(pa_context_set_sink_port_by_name(m_context, m_sinkName.c_str(), m_originalPort.c_str(),
                                                 &successCallback, this));
    m_sinkName.clear();
    m_originalPort.clear();
    m_earpiecePort.clear();
    m_speakerPort.clear();
    m_hasEarpiece = false;
    if (m_context) {
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = nullptr;
    }
    pa_threaded_mainloop_unlock(m_loop);
    pa_threaded_mainloop_stop(m_loop);
    m_apm = nullptr;
    if (m_denoise) {
        rnnoise_destroy(m_denoise);
        m_denoise = nullptr;
    }
    pa_threaded_mainloop_free(m_loop);
    m_loop = nullptr;
    std::lock_guard<std::mutex> lock(m_mixLock);
    m_sources.clear();
}

void AudioIO::setCaptureEnabled(bool enabled)
{
    if (!m_loop || !m_record)
        return;
    pa_threaded_mainloop_lock(m_loop);
    unref(pa_stream_cork(m_record, enabled ? 0 : 1, nullptr, nullptr));
    m_captureBuffer.clear();
    pa_threaded_mainloop_unlock(m_loop);
}

void AudioIO::setPlaybackEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mixLock);
    m_playbackEnabled = enabled;
    if (!enabled)
        m_sources.clear();
}

void AudioIO::onRead(size_t)
{
    const void* data = nullptr;
    size_t bytes = 0;
    if (!m_record)
        return;
    while (pa_stream_readable_size(m_record) > 0) {
        if (pa_stream_peek(m_record, &data, &bytes) < 0)
            return;
        if (bytes == 0)
            break;
        if (data) {
            const auto* samples = static_cast<const int16_t*>(data);
            m_captureBuffer.insert(m_captureBuffer.end(), samples, samples + bytes / sizeof(int16_t));
        } else {
            // A hole: silence.
            m_captureBuffer.insert(m_captureBuffer.end(), bytes / sizeof(int16_t), 0);
        }
        pa_stream_drop(m_record);
    }
    size_t offset = 0;
    while (m_captureBuffer.size() - offset >= size_t(FrameSamples)) {
        if (m_capture)
        {
            processCapture(m_captureBuffer.data() + offset);
            m_capture(m_captureBuffer.data() + offset);
        }
        offset += FrameSamples;
    }
    m_captureBuffer.erase(m_captureBuffer.begin(), m_captureBuffer.begin() + long(offset));
}

void AudioIO::play(uint32_t source, const int16_t* samples)
{
    std::lock_guard<std::mutex> lock(m_mixLock);
    if (!m_playbackEnabled)
        return;
    Source& s = m_sources[source];
    s.frames.emplace_back(samples, samples + FrameSamples * 2);
    while (s.frames.size() > MaxQueuedFrames)
        s.frames.pop_front();
    if (s.frames.size() >= PrimeFrames)
        s.primed = true;
}

void AudioIO::removeSource(uint32_t source)
{
    std::lock_guard<std::mutex> lock(m_mixLock);
    m_sources.erase(source);
}

void AudioIO::mix(int16_t* out, size_t frames)
{
    const size_t samples = frames * 2;
    std::vector<int32_t> sum(samples, 0);
    {
        std::lock_guard<std::mutex> lock(m_mixLock);
        // Mix whole 20 ms frames; a partial request gets the start of one.
        for (auto& [id, source] : m_sources) {
            if (!source.primed)
                continue;
            size_t filled = 0;
            while (filled < samples && !source.frames.empty()) {
                std::vector<int16_t>& frame = source.frames.front();
                const size_t take = std::min(samples - filled, frame.size());
                for (size_t i = 0; i < take; ++i)
                    sum[filled + i] += frame[i];
                filled += take;
                if (take == frame.size())
                    source.frames.pop_front();
                else
                    frame.erase(frame.begin(), frame.begin() + long(take));
            }
            if (source.frames.empty())
                source.primed = false; // wait for the buffer to refill
        }
    }
    for (size_t i = 0; i < samples; ++i)
        out[i] = int16_t(std::clamp<int32_t>(sum[i], INT16_MIN, INT16_MAX));
}

void AudioIO::onWrite(size_t bytes)
{
    void* buffer = nullptr;
    size_t size = bytes;
    if (!m_playback)
        return;
    if (pa_stream_begin_write(m_playback, &buffer, &size) < 0 || !buffer)
        return;
    const size_t frames = size / (2 * sizeof(int16_t));
    mix(static_cast<int16_t*>(buffer), frames);
    analyzePlayback(static_cast<const int16_t*>(buffer), frames);
    pa_stream_write(m_playback, buffer, size, nullptr, 0, PA_SEEK_RELATIVE);
}

// Earpiece / loudspeaker

void AudioIO::queryPorts()
{
    // The call's output device is the default sink.
    waitFor(pa_context_get_sink_info_by_name(m_context, "@DEFAULT_SINK@", &sinkInfoCallback, this));
}

void AudioIO::onSinkInfo(const pa_sink_info* info, int eol)
{
    if (eol || !info) {
        pa_threaded_mainloop_signal(m_loop, 0);
        return;
    }
    m_sinkName = info->name;
    m_originalPort = info->active_port ? info->active_port->name : "";
    for (uint32_t i = 0; i < info->n_ports; ++i) {
        const std::string name = info->ports[i]->name;
        auto endsWith = [&name](const char* suffix) {
            const size_t n = std::strlen(suffix);
            return name.size() >= n && name.compare(name.size() - n, n, suffix) == 0;
        };
        // Android audio HALs also have "output-speaker_safe",
        // "output-a2dp_speaker" (Bluetooth) and the like.
        if (info->ports[i]->available == PA_PORT_AVAILABLE_NO)
            continue;
        if (endsWith("-earpiece"))
            m_earpiecePort = name;
        else if (endsWith("-speaker"))
            m_speakerPort = name;
    }
    m_hasEarpiece = !m_earpiecePort.empty() && !m_speakerPort.empty();
    // A parked output (droid, between profiles) is not worth going back to.
    if (m_originalPort.find("parking") != std::string::npos)
        m_originalPort = m_speakerPort;
}

bool AudioIO::setSpeaker(bool speaker)
{
    if (!m_loop || !m_hasEarpiece)
        return false;
    pa_threaded_mainloop_lock(m_loop);
    const std::string& port = speaker ? m_speakerPort : m_earpiecePort;
    unref(pa_context_set_sink_port_by_name(m_context, m_sinkName.c_str(), port.c_str(), nullptr, nullptr));
    pa_threaded_mainloop_unlock(m_loop);
    return true;
}

void AudioIO::waitFor(pa_operation* op)
{
    if (!op)
        return;
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(m_loop);
    pa_operation_unref(op);
}

// Echo cancellation, noise suppression, gain control

namespace {

constexpr int ChunkSamples = AudioIO::SampleRate / 100; // WebRTC and RNNoise work on 10 ms

}

void AudioIO::setUpProcessing(bool denoise)
{
    m_apm = webrtc::AudioProcessingBuilder().Create();
    if (!m_apm)
        return;
    webrtc::AudioProcessing::Config config;
    config.high_pass_filter.enabled = true;
    // AEC3; mobile mode would be the older AECM.
    config.echo_canceller.enabled = true;
    config.echo_canceller.mobile_mode = false;
    config.noise_suppression.enabled = !denoise;
    config.noise_suppression.level = webrtc::AudioProcessing::Config::NoiseSuppression::kHigh;
    // AGC2 only adapts to speech, so it doesn't boost leftover echo.
    config.gain_controller2.enabled = true;
    config.gain_controller2.adaptive_digital.enabled = true;
    config.residual_echo_detector.enabled = false;
    m_apm->ApplyConfig(config);
}

int AudioIO::streamDelayMs()
{
    // Output latency plus input latency.
    pa_usec_t total = 0;
    for (pa_stream* stream : {m_playback, m_record}) {
        pa_usec_t latency = 0;
        int negative = 0;
        if (stream && pa_stream_get_latency(stream, &latency, &negative) == 0 && !negative)
            total += latency;
    }
    return int(std::min<pa_usec_t>(total / 1000, 500));
}

void AudioIO::processCapture(int16_t* samples)
{
    if (!m_apm && !m_denoise)
        return;
    const webrtc::StreamConfig config(SampleRate, 1);
    float chunk[ChunkSamples];
    float* channels[] = {chunk};
    for (int offset = 0; offset < FrameSamples; offset += ChunkSamples) {
        for (int i = 0; i < ChunkSamples; ++i)
            chunk[i] = samples[offset + i] / 32768.0f;
        if (m_apm) {
            m_apm->set_stream_delay_ms(streamDelayMs());
            m_apm->ProcessStream(channels, config, config, channels);
        }
        if (m_denoise) {
            // RNNoise takes samples in 16-bit range.
            for (float& sample : chunk)
                sample *= 32768.0f;
            rnnoise_process_frame(m_denoise, chunk, chunk);
            for (float& sample : chunk)
                sample /= 32768.0f;
        }
        for (int i = 0; i < ChunkSamples; ++i)
            samples[offset + i] = int16_t(std::clamp(chunk[i] * 32768.0f, -32768.0f, 32767.0f));
    }
}

void AudioIO::analyzePlayback(const int16_t* stereo, size_t frames)
{
    if (!m_apm)
        return;
    // The echo reference: what goes to the loudspeaker, as mono.
    for (size_t i = 0; i < frames; ++i)
        m_renderBuffer.push_back((stereo[2 * i] + stereo[2 * i + 1]) / 65536.0f);
    const webrtc::StreamConfig config(SampleRate, 1);
    size_t offset = 0;
    while (m_renderBuffer.size() - offset >= size_t(ChunkSamples)) {
        float* channels[] = {m_renderBuffer.data() + offset};
        m_apm->ProcessReverseStream(channels, config, config, channels);
        offset += ChunkSamples;
    }
    m_renderBuffer.erase(m_renderBuffer.begin(), m_renderBuffer.begin() + long(offset));
}
