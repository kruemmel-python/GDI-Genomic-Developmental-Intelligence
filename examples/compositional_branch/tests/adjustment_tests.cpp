#include "adjustment.hpp"

#include <iostream>

int main() {
    const bool accepted = retainedPressure(3, 5) == 3 && retainedPressure(8, 5) == 3 && retainedPressure(5, 5) == 5;
    if (!accepted) {
        std::cerr << "adjustment contract violated\n";
        return 1;
    }
    return 0;
}
