#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <memory>
#include <vector>

#include <api/scoped_refptr.h>

struct pa_threaded_mainloop;
struct pa_context;
struct pa_stream;
struct pa_operation;

namespace webrtc { class AudioProcessing; }
struct DenoiseState;

// Call audio through PulseAudio: the microphone in 20 ms frames of 48 kHz
// mono, and the other participants mixed to the output, each with a small
// jitter buffer. On phones the output switches between loudspeaker and
// earpiece through the sink's ports.
//
// The microphone goes through WebRTC's echo canceller (with what we play as
// the reference), noise suppression and gain control. With `denoise`,
// RNNoise replaces WebRTC's noise suppression.
class AudioIO
{
public:
    static constexpr int SampleRate = 48000;
    static constexpr int FrameSamples = 960; // 20 ms

    // Called on PulseAudio's thread with FrameSamples mono samples.
    using CaptureCallback = std::function<void(const int16_t* samples)>;

    AudioIO();
    ~AudioIO();

    bool start(CaptureCallback capture, bool processing = true, bool denoise = true);
    void stop();
    std::string error() const { return m_error; }

    void setCaptureEnabled(bool enabled);
    void setPlaybackEnabled(bool enabled);

    // A participant's decoded frame: FrameSamples stereo (interleaved).
    void play(uint32_t source, const int16_t* samples);
    void removeSource(uint32_t source);

    // Loudspeaker (true, where calls start) or earpiece (false); false when
    // the output has no such choice (desktops).
    bool setSpeaker(bool speaker);
    bool hasEarpiece() const { return m_hasEarpiece; }

    // Called by PulseAudio's callbacks.
    void onContextState();
    void onRead(size_t bytes);
    void onWrite(size_t bytes);
    void onSinkInfo(const struct pa_sink_info* info, int eol);

private:
    bool waitForContext();
    void mix(int16_t* out, size_t frames);
    void queryPorts();
    void setUpProcessing(bool denoise);
    void processCapture(int16_t* samples);
    void analyzePlayback(const int16_t* stereo, size_t frames);
    int streamDelayMs();
    void waitFor(pa_operation* op);

    pa_threaded_mainloop* m_loop = nullptr;
    pa_context* m_context = nullptr;
    pa_stream* m_record = nullptr;
    pa_stream* m_playback = nullptr;
    CaptureCallback m_capture;
    std::vector<int16_t> m_captureBuffer;

    // Only used on PulseAudio's thread. Works on 10 ms chunks; the rest of
    // the playback mix waits in m_renderBuffer.
    rtc::scoped_refptr<webrtc::AudioProcessing> m_apm;
    std::vector<float> m_renderBuffer;
    DenoiseState* m_denoise = nullptr;
    std::string m_error;

    struct Source {
        std::deque<std::vector<int16_t>> frames;
        bool primed = false;
    };
    std::mutex m_mixLock;
    std::map<uint32_t, Source> m_sources;
    bool m_playbackEnabled = true;

    // The output device's ports, to switch earpiece / loudspeaker.
    std::string m_sinkName;
    std::string m_originalPort;
    std::string m_earpiecePort;
    std::string m_speakerPort;
    bool m_hasEarpiece = false;
};
