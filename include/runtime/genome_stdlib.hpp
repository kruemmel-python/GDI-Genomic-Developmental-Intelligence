#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace genomic {

class GenomeStdLib {
public:
    // Math & Numerics
    static int64_t gcd(int64_t a, int64_t b);
    static int64_t lcm(int64_t a, int64_t b);
    static double clamp(double val, double minVal, double maxVal);
    static int64_t clamp(int64_t val, int64_t minVal, int64_t maxVal);

    // Algorithms
    static void sort(std::vector<int64_t>& vec);
    static bool binarySearch(const std::vector<int64_t>& sortedVec, int64_t target);
    static std::vector<int64_t> dijkstraShortestPath(
        size_t nodeCount,
        const std::vector<std::tuple<size_t, size_t, double>>& edges,
        size_t startNode,
        size_t endNode
    );

    // Deterministic State Machine
    struct StateTransition {
        uint32_t fromState{0};
        uint32_t eventId{0};
        uint32_t toState{0};
    };

    class StateMachine {
    public:
        explicit StateMachine(uint32_t initialState = 0) : currentState_(initialState) {}
        void addTransition(uint32_t from, uint32_t event, uint32_t to);
        bool triggerEvent(uint32_t event);
        uint32_t getCurrentState() const noexcept { return currentState_; }

    private:
        uint32_t currentState_{0};
        std::vector<StateTransition> transitions_;
    };
};

} // namespace genomic
