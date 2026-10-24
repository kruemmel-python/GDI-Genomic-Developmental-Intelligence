#include "score.hpp"

#include <iostream>

int main() {
    constexpr int incidentWeight = 3;
    const bool accepted = settlementScore(2, 4) == 2 + 4 * incidentWeight
        && settlementScore(5, 1) == 5 + incidentWeight
        && settlementScore(9, 0) == 9;
    if (!accepted) {
        std::cerr << "score contract violated\n";
        return 1;
    }
    return 0;
}
