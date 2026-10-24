#include "recursive.hpp"

#include <iostream>

int main() {
    const bool accepted = factorialWeight(0) == 1 && factorialWeight(1) == 1
        && factorialWeight(4) == 24 && factorialWeight(6) == 720;
    if (!accepted) {
        std::cerr << "recursive contract violated\n";
        return 1;
    }
    return 0;
}
