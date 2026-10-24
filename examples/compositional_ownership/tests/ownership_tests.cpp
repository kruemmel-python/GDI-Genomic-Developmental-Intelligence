#include "ownership.hpp"

#include <iostream>
#include <memory>

int main() {
    const auto original = std::make_unique<int>(9);
    const auto duplicate = duplicateWeight(original);
    const auto empty = duplicateWeight(nullptr);
    const bool accepted = duplicate && *duplicate == 9 && duplicate.get() != original.get() && empty == nullptr;
    if (!accepted) {
        std::cerr << "ownership contract violated\n";
        return 1;
    }
    return 0;
}
