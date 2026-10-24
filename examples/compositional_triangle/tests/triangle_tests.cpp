#include "triangle.hpp"

#include <iostream>

int main() {
    const bool accepted = staircaseTiles(1) == 1 && staircaseTiles(4) == 10 && staircaseTiles(7) == 28;
    if (!accepted) {
        std::cerr << "staircase contract violated\n";
        return 1;
    }
    return 0;
}
