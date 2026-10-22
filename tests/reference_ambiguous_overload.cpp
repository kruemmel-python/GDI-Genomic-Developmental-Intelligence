#include <cstdint>
void choose(std::int32_t, double);
void choose(std::int64_t, std::int32_t);
int main() { choose(std::int32_t{1}, std::int32_t{2}); }
