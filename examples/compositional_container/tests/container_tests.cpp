#include "container.hpp"

#include <iostream>
#include <vector>

int main() {
    const bool accepted = collectedWeight({}) == 0 && collectedWeight({2, 3, 5}) == 10
        && collectedWeight({-4, 7}) == 3;
    if (!accepted) {
        std::cerr << "container contract violated\n";
        return 1;
    }
    return 0;
}
