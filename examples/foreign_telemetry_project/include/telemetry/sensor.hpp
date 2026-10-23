#pragma once

namespace telemetry {

struct SensorReading {
    long long base;
    long long delta;
};

long long calibrate_reading(SensorReading r);
long long total_signal(SensorReading r);

} // namespace telemetry
