#pragma once
#include <optional>
#include <string>
std::optional<std::string> readPersistedState(const std::string& path);
