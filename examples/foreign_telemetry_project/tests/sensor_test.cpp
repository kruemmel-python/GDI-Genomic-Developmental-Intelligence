#include "telemetry/sensor.hpp"

int main() {
    if (telemetry::calibrate_reading(telemetry::SensorReading{40, 15}) != 25) return 1;
    if (telemetry::total_signal(telemetry::SensorReading{10, 5}) != 15) return 2;
    if (telemetry::total_signal(telemetry::SensorReading{40, 15}) != 55) return 3;
    return 0;
}
