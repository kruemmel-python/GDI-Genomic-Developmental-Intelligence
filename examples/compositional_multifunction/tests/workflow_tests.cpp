#include "workflow.hpp"

#include <iostream>
#include <vector>

int main() {
    const bool accepted = retainedPressure(3, 5) == 3 && retainedPressure(8, 5) == 3
        && totalWeight({2, 3, 5}) == 10 && totalWeight({-2, 4}) == 2;
    if (!accepted) {
        std::cerr << "workflow contract violated\n";
        return 1;
    }
    return 0;
}
