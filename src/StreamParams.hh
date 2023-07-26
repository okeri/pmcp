#pragma once

enum class SampleFormat { None, U8, S8, S16, S24, S32, F32, F64 };

struct StreamParams {
    SampleFormat format{SampleFormat::None};
    unsigned channels{2};
    long rate{44100};
};
