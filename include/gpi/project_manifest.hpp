#pragma once

#include "gpi/generative_project.hpp"
#include "gpi/project_verifier.hpp"

#include <filesystem>

namespace genomic::gpi {

struct LoadedProjectSpecification {
    GenerativeProjectGraph graph;
    ProjectVerificationContract contract;
};

// Loads a strict, line-oriented project description. Source and fixture
// contents are read exactly from projectRoot; escaping paths are rejected.
LoadedProjectSpecification loadProjectManifest(
    const std::filesystem::path& manifest,
    const std::filesystem::path& projectRoot,
    Cpp20SourceGenome sourceGenome);

} // namespace genomic::gpi
