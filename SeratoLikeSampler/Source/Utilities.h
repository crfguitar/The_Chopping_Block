
#pragma once
#include <juce_core/juce_core.h>
namespace util {
    template <typename T>
    inline T clamp(T v, T lo, T hi) { return juce::jlimit(lo, hi, v); }
}
