#pragma once
// Debug-only scoped timing; compiles to nothing unless PERF_DEBUG is defined.
#ifdef PERF_DEBUG
#include <Arduino.h>

namespace perf {

struct ScopedTimer {
    const char* label;
    uint32_t startUs;
    explicit ScopedTimer(const char* l) : label(l), startUs(micros()) {}
    ~ScopedTimer() {
        uint32_t elapsed = micros() - startUs;
        Serial.printf("[perf] %s: %lu us\n", label, (unsigned long)elapsed);
    }
};

} // namespace perf

#define PERF_CONCAT_INNER(a, b) a##b
#define PERF_CONCAT(a, b) PERF_CONCAT_INNER(a, b)
#define PERF_SCOPE(label) perf::ScopedTimer PERF_CONCAT(_perfTimer, __LINE__)(label)

#else
#define PERF_SCOPE(label)
#endif
