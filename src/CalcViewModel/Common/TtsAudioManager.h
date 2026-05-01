#pragma once

#include <mutex>
#include <queue>
#include <map>
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Media.Playback.h>

namespace CalculatorApp::Common
{
    class TtsAudioManager
    {
    public:
        static TtsAudioManager& Get();

        void Enqueue(winrt::hstring const& text);
        winrt::Windows::Foundation::IAsyncAction InitCache();

    private:
        TtsAudioManager();
        ~TtsAudioManager() = default;

        TtsAudioManager(const TtsAudioManager&) = delete;
        TtsAudioManager& operator=(const TtsAudioManager&) = delete;

        void PlayNext();
        void OnMediaEnded(winrt::Windows::Media::Playback::MediaPlayer const& sender, winrt::Windows::Foundation::IInspectable const& args);

        winrt::Windows::Media::SpeechSynthesis::SpeechSynthesizer m_synthesizer{ nullptr };
        winrt::Windows::Media::Playback::MediaPlayer m_mediaPlayer{ nullptr };

        std::mutex m_mutex;
        std::queue<winrt::hstring> m_queue;
        std::map<winrt::hstring, winrt::Windows::Media::SpeechSynthesis::SpeechSynthesisStream> m_cache;
        bool m_isPlaying = false;

        winrt::event_token m_mediaEndedToken;
    };
}
