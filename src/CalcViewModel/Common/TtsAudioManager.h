#pragma once

#include <string>

namespace CalculatorApp::ViewModel::Common
{
    public ref class TtsAudioManager sealed
    {
    public:
        static void InitCache();
        static void Enqueue(Platform::String^ text);

    private:
        static void PlayNext();
    };
}
