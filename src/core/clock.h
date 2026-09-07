#pragma once
#include <chrono>
#include <QtGlobal>
namespace wheel {
inline qint64 monotonicNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
