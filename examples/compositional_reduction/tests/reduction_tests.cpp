#include "reduction.hpp"

#include <iostream>

int main() {
    const bool accepted = sharedStride(48, 18) == 6 && sharedStride(54, 24) == 6 && sharedStride(17, 13) == 1;
    if (!accepted) {
        std::cerr << "reduction contract violated\n";
        return 1;
    }
    return 0;
}
