#include "pch.h"
#include "TtsAudioManager.h"
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace CalculatorApp::ViewModel::Common;
using namespace Platform;
using namespace std;

static winrt::Windows::Media::Playback::MediaPlayer g_mediaPlayer{ nullptr };
static winrt::Windows::Media::SpeechSynthesis::SpeechSynthesizer g_synthesizer{ nullptr };
static winrt::Windows::Media::Playback::MediaPlaybackList g_playbackList{ nullptr };

void TtsAudioManager::InitCache()
{
    if (!g_mediaPlayer)
    {
        try {
            g_mediaPlayer = winrt::Windows::Media::Playback::MediaPlayer();
            g_mediaPlayer.AudioCategory(winrt::Windows::Media::Playback::MediaPlayerAudioCategory::Speech);
            g_mediaPlayer.Volume(1.0);

            g_playbackList = winrt::Windows::Media::Playback::MediaPlaybackList();
            g_mediaPlayer.Source(g_playbackList);

            g_synthesizer = winrt::Windows::Media::SpeechSynthesis::SpeechSynthesizer();
            
            OutputDebugStringW(L"TTS: System Initialized with MediaPlaybackList\n");
        }
        catch (...) {
            OutputDebugStringW(L"TTS: Initialization Failed\n");
        }
    }
}

void TtsAudioManager::Enqueue(Platform::String^ text)
{
    if (!g_synthesizer || !g_playbackList || text == nullptr || text->IsEmpty()) return;

    std::wstring wText(text->Data());
    
    concurrency::create_task([wText]() {
        try {
            auto stream = g_synthesizer.SynthesizeTextToStreamAsync(wText).get();
            winrt::Windows::Media::Core::MediaSource source = winrt::Windows::Media::Core::MediaSource::CreateFromStream(stream, stream.ContentType());
            winrt::Windows::Media::Playback::MediaPlaybackItem item{ source };

            // 自动加入播放列表并排队
            g_playbackList.Items().Append(item);
            
            // 如果当前没在播，就开始播
            if (g_mediaPlayer.PlaybackSession().PlaybackState() != winrt::Windows::Media::Playback::MediaPlaybackState::Playing)
            {
                g_mediaPlayer.Play();
            }
            
            OutputDebugStringW((L"TTS Queued: " + wText + L"\n").c_str());
        }
        catch (...) {
            OutputDebugStringW(L"TTS: Synthesis/Enqueue Failed\n");
        }
    });
}
