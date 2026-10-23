#include "telemetry/sensor.hpp"

namespace telemetry {

long long calibrate_reading(SensorReading r) {
    return r.base - r.delta;
}

} // namespace telemetry
