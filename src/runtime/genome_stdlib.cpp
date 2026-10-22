#include "runtime/genome_stdlib.hpp"
#include <queue>
#include <limits>

namespace genomic {

int64_t GenomeStdLib::gcd(int64_t a, int64_t b) {
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return std::abs(a);
}

int64_t GenomeStdLib::lcm(int64_t a, int64_t b) {
    if (a == 0 || b == 0) return 0;
    return std::abs(a * b) / gcd(a, b);
}

double GenomeStdLib::clamp(double val, double minVal, double maxVal) {
    return std::clamp(val, minVal, maxVal);
}

int64_t GenomeStdLib::clamp(int64_t val, int64_t minVal, int64_t maxVal) {
    return std::clamp(val, minVal, maxVal);
}

void GenomeStdLib::sort(std::vector<int64_t>& vec) {
    std::sort(vec.begin(), vec.end());
}

bool GenomeStdLib::binarySearch(const std::vector<int64_t>& sortedVec, int64_t target) {
    return std::binary_search(sortedVec.begin(), sortedVec.end(), target);
}

std::vector<int64_t> GenomeStdLib::dijkstraShortestPath(
    size_t nodeCount,
    const std::vector<std::tuple<size_t, size_t, double>>& edges,
    size_t startNode,
    size_t endNode
) {
    std::vector<double> dist(nodeCount, std::numeric_limits<double>::infinity());
    std::vector<int64_t> prev(nodeCount, -1);
    dist[startNode] = 0.0;

    // Adjacency list
    std::vector<std::vector<std::pair<size_t, double>>> adj(nodeCount);
    for (const auto& [u, v, w] : edges) {
        if (u < nodeCount && v < nodeCount) {
            adj[u].push_back({v, w});
            adj[v].push_back({u, w}); // undirected graph
        }
    }

    using PDI = std::pair<double, size_t>;
    std::priority_queue<PDI, std::vector<PDI>, std::greater<PDI>> pq;
    pq.push({0.0, startNode});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) continue;
        if (u == endNode) break;

        for (const auto& [v, w] : adj[u]) {
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = static_cast<int64_t>(u);
                pq.push({dist[v], v});
            }
        }
    }

    std::vector<int64_t> path;
    if (dist[endNode] == std::numeric_limits<double>::infinity()) {
        return path; // no path
    }

    for (int64_t at = static_cast<int64_t>(endNode); at != -1; at = prev[at]) {
        path.push_back(at);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void GenomeStdLib::StateMachine::addTransition(uint32_t from, uint32_t event, uint32_t to) {
    transitions_.push_back(StateTransition{from, event, to});
}

bool GenomeStdLib::StateMachine::triggerEvent(uint32_t event) {
    for (const auto& t : transitions_) {
        if (t.fromState == currentState_ && t.eventId == event) {
            currentState_ = t.toState;
            return true;
        }
    }
    return false;
}

} // namespace genomic
