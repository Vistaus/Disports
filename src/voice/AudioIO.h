#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <memory>
#include <vector>

struct pa_threaded_mainloop;
struct pa_context;
struct pa_stream;
struct pa_operation;

namespace webrtc { class AudioProcessing; }

// Call audio through PulseAudio (the "audio" and "microphone" AppArmor
// policy groups allow its socket to confined apps): the microphone in 20 ms
// frames of 48 kHz mono, and a mix of the other participants to the output,
// one small jitter buffer each. Both streams have media.role=phone. On
// phones, the output switches between loudspeaker and earpiece through the
// sink's ports.
//
// The microphone goes through WebRTC's audio processing (the library
// PulseAudio uses too, already on Ubuntu Touch): echo cancellation, with
// what we play as its reference, noise suppression, gain control.
class AudioIO
{
public:
    static constexpr int SampleRate = 48000;
    static constexpr int FrameSamples = 960; // 20 ms

    // Called on PulseAudio's thread with FrameSamples mono samples.
    using CaptureCallback = std::function<void(const int16_t* samples)>;

    AudioIO();
    ~AudioIO();

    bool start(CaptureCallback capture, bool processing = true);
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
    void setUpProcessing();
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

    // WebRTC audio processing, used on PulseAudio's thread only; it works
    // on 10 ms chunks. The playback mix (mono) waits in m_renderBuffer.
    std::unique_ptr<webrtc::AudioProcessing> m_apm;
    std::vector<float> m_renderBuffer;
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
