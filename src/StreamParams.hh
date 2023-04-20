#pragma once

enum class SampleFormat { None, U8, S8, S16, S32, F32, F64 };

struct StreamParams {
    SampleFormat format;
    unsigned channels;
    long rate;
};
