#pragma once

enum class SampleFormat { None, U8, S8, S16, S24, S32, F32, F64 };

constexpr auto DefaultRate = 44100;

struct StreamParams {
    SampleFormat format{SampleFormat::None};
    unsigned channelCount{2};
    long rate{DefaultRate};
    double volume{1.};
};
