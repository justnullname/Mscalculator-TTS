#include "pch.h"
#include "TtsAudioManager.h"
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <map>
#include <vector>
#include <memory>

using namespace CalculatorApp::ViewModel::Common;
using namespace Platform;
using namespace std;
using namespace winrt::Windows::Media::Playback;
using namespace winrt::Windows::Media::SpeechSynthesis;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;

// 播放器状态封装
struct ManagedPlayer {
    MediaPlayer player{ nullptr };
    bool isPlaying = false;
    winrt::event_token endedToken;
};

// 缓存项：包含音频流及其 MIME 类型
struct CachedAudio {
    InMemoryRandomAccessStream stream{ nullptr };
    winrt::hstring contentType;
};

// 全局静态资源
static map<wstring, CachedAudio> g_audioCache;
static mutex g_cacheMutex;
static vector<shared_ptr<ManagedPlayer>> g_playerPool;
static mutex g_poolMutex;
static SpeechSynthesizer g_synthesizer{ nullptr };

// 后台工作线程管理
static queue<wstring> g_pendingTexts;
static mutex g_workerMutex;
static condition_variable g_workerCv;
static bool g_workerRunning = false;
static thread g_workerThread;

// 从池中获取或创建空闲播放器
static shared_ptr<ManagedPlayer> GetIdlePlayer()
{
    lock_guard<mutex> lock(g_poolMutex);
    for (auto& mp : g_playerPool)
    {
        if (!mp->isPlaying)
        {
            mp->isPlaying = true;
            return mp;
        }
    }

    // 创建新播放器实例
    auto mp = make_shared<ManagedPlayer>();
    mp->player = MediaPlayer();
    mp->player.AudioCategory(MediaPlayerAudioCategory::Speech);
    mp->isPlaying = true;

    // 监听结束事件以标记空闲状态
    mp->endedToken = mp->player.MediaEnded([mp](MediaPlayer const&, auto const&) {
        mp->isPlaying = false;
    });

    g_playerPool.push_back(mp);
    return mp;
}

// 预缓存常用字符
static winrt::Windows::Foundation::IAsyncAction PreCacheAsync()
{
    // 定义需要预缓存的常用按键文本
    vector<wstring> targets = {
        L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9",
        L".", L"+", L"-", L"×", L"÷", L"=", L"±", L"backspace", L"clear",
        L"加", L"减", L"乘", L"除", L"等于", L"点", L"清除"
    };

    for (const auto& text : targets)
    {
        try
        {
            auto stream = co_await g_synthesizer.SynthesizeTextToStreamAsync(text);
            InMemoryRandomAccessStream memoryStream;
            co_await RandomAccessStream::CopyAsync(stream, memoryStream);
            
            CachedAudio cached;
            cached.stream = memoryStream;
            cached.contentType = stream.ContentType();

            lock_guard<mutex> lock(g_cacheMutex);
            g_audioCache[text] = cached;
        }
        catch (...)
        {
            // 缓存失败不影响后续，仅作为优化
        }
    }
}

// 后台工作线程：实现并行触发逻辑
void TtsWorker()
{
    while (g_workerRunning)
    {
        wstring text;
        {
            unique_lock<mutex> lock(g_workerMutex);
            g_workerCv.wait(lock, [] { return !g_pendingTexts.empty() || !g_workerRunning; });
            if (!g_workerRunning) break;
            text = g_pendingTexts.front();
            g_pendingTexts.pop();
        }

        try
        {
            IRandomAccessStream playStream = nullptr;
            winrt::hstring contentType;
            {
                lock_guard<mutex> lock(g_cacheMutex);
                auto it = g_audioCache.find(text);
                if (it != g_audioCache.end())
                {
                    // 克隆流以支持多个播放器同时使用
                    playStream = it->second.stream.CloneStream();
                    contentType = it->second.contentType;
                }
            }

            if (!playStream)
            {
                // 未命中缓存，实时合成（较慢）
                auto synthStream = g_synthesizer.SynthesizeTextToStreamAsync(text).get();
                contentType = synthStream.ContentType();
                synthStream.Seek(0);
                
                InMemoryRandomAccessStream memoryStream;
                RandomAccessStream::CopyAsync(synthStream, memoryStream).get();
                memoryStream.Seek(0);
                playStream = memoryStream;
            }

            if (playStream)
            {
                playStream.Seek(0);
                auto source = MediaSource::CreateFromStream(playStream, contentType);
                
                auto managedPlayer = GetIdlePlayer();
                managedPlayer->player.Source(source);
                managedPlayer->player.Play();

                // 核心：设置 50ms 的触发间隔实现并行重叠
                this_thread::sleep_for(chrono::milliseconds(50));
            }
        }
        catch (...)
        {
            // 忽略单次播放失败
        }
    }
}

void TtsAudioManager::InitCache()
{
    if (!g_synthesizer)
    {
        try
        {
            g_synthesizer = SpeechSynthesizer();
            
            // 启动后台线程
            g_workerRunning = true;
            g_workerThread = thread(TtsWorker);

            // 异步执行预缓存
            PreCacheAsync();
        }
        catch (...) {}
    }
}

void TtsAudioManager::Enqueue(Platform::String^ text)
{
    if (!g_workerRunning || text == nullptr || text->IsEmpty()) return;

    {
        lock_guard<mutex> lock(g_workerMutex);
        g_pendingTexts.push(text->Data());
    }
    g_workerCv.notify_one();
}
