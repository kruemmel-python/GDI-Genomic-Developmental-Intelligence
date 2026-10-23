#include "telemetry/sensor.hpp"
#include <iostream>

int main() {
    telemetry::SensorReading sample{40, 15};
    std::cout << "CALIBRATED=" << telemetry::calibrate_reading(sample) << "\n";
    std::cout << "TOTAL_SIGNAL=" << telemetry::total_signal(sample) << "\n";
    return 0;
}
