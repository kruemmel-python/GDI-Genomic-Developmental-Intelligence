#include "pressure.hpp"

#include <iostream>

int main() {
    constexpr int shortageWeight = 3;
    const bool accepted = inventoryPressure(1, 5) == 1 + 5 * shortageWeight
        && inventoryPressure(7, 2) == 7 + 2 * shortageWeight
        && inventoryPressure(4, 0) == 4;
    if (!accepted) {
        std::cerr << "pressure contract violated\n";
        return 1;
    }
    return 0;
}
