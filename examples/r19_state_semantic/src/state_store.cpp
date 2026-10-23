#include "state_store.hpp"
#include <fstream>
std::optional<std::string> readPersistedState(const std::string& path) { std::ifstream input(path); std::string value; if(!std::getline(input,value)) return std::nullopt; return value; }
