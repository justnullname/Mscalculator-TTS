#include "pch.h"
#include "TtsAudioManager.h"
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Media::Playback;
using namespace Windows::Foundation;

namespace CalculatorApp::Common
{
    TtsAudioManager& TtsAudioManager::Get()
    {
        static TtsAudioManager instance;
        return instance;
    }

    TtsAudioManager::TtsAudioManager()
    {
        m_synthesizer = SpeechSynthesizer();
        m_mediaPlayer = MediaPlayer();
        m_mediaPlayer.AutoPlay(true);
        m_mediaEndedToken = m_mediaPlayer.MediaEnded({ this, &TtsAudioManager::OnMediaEnded });
    }

    IAsyncAction TtsAudioManager::InitCache()
    {
        std::vector<hstring> defaultTexts = { L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"+", L"-", L"*", L"/", L"=", L".", L"C", L"CE" };

        for (const auto& text : defaultTexts)
        {
            if (m_cache.find(text) == m_cache.end())
            {
                auto stream = co_await m_synthesizer.SynthesizeTextToStreamAsync(text);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[text] = stream;
            }
        }
    }

    void TtsAudioManager::Enqueue(hstring const& text)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(text);

        if (!m_isPlaying)
        {
            PlayNext();
        }
    }

    void TtsAudioManager::PlayNext()
    {
        if (m_queue.empty())
        {
            m_isPlaying = false;
            return;
        }

        m_isPlaying = true;
        hstring text = m_queue.front();
        m_queue.pop();

        SpeechSynthesisStream stream{ nullptr };
        {
            if (m_cache.find(text) != m_cache.end())
            {
                stream = m_cache[text];
            }
        }

        if (stream)
        {
            stream.Seek(0);
            m_mediaPlayer.Source(MediaSource::CreateFromStream(stream, stream.ContentType()));
        }
        else
        {
            // Synthesize on the fly if not cached
            auto op = m_synthesizer.SynthesizeTextToStreamAsync(text);
            op.Completed(
                [this](IAsyncOperation<SpeechSynthesisStream> const& sender, AsyncStatus const status)
                {
                    if (status == AsyncStatus::Completed)
                    {
                        auto newStream = sender.GetResults();
                        m_mediaPlayer.Source(MediaSource::CreateFromStream(newStream, newStream.ContentType()));
                    }
                    else
                    {
                        // Fallback on error
                        std::lock_guard<std::mutex> lock(m_mutex);
                        PlayNext();
                    }
                });
        }
    }

    void TtsAudioManager::OnMediaEnded(MediaPlayer const& sender, IInspectable const& args)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        PlayNext();
    }
}
