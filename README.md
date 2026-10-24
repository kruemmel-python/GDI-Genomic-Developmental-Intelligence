# GDI — Genomic Developmental Intelligence

**Windows x64 · C++20 · protected-project autonomous development research platform**

GDI is a native C++20 research system for safely analysing, changing and verifying software projects. Its architecture combines an immutable synthetic genome, regulatory capability activation, build-contract reconstruction, semantic project analysis, isolated experimentation and persistent experience.

GDI never edits an imported project in place. Every attempted change is created in a separate workspace, compiled and tested with the project’s real build contract. A result is reported as successful only when the relevant build and behavioural evidence succeed.

## Current capabilities

| Capability | Evidence requirement |
| --- | --- |
| Build-contract reconstruction | Detects supported CMake, MSBuild, Ninja, Make and direct compiler workflows, targets and test commands. |
| Protected foreign-project analysis | Creates isolated copies, validates paths, rejects unsafe links and compares original-tree SHA-256 manifests. |
| Semantic integration analysis | Establishes source, call-flow and state evidence before selecting a supported integration point. |
| Persistent development experience | Stores successful and failed, provenance-bound attempts and uses them to guide later work. |
| Adaptive experiments | Tests bounded, observable candidate changes in isolated workspaces and preserves negative evidence. |
| Inductive expression synthesis | Discovers safe integer return stubs, derives variables/constants from the project, generates candidate expressions, and accepts only candidates proven by the project’s own tests. |

The inductive synthesizer is deliberately evidence-led. It does not contain project names, function names or a prewritten solution for the target project. It forms candidate programs during the run, recompiles and executes each candidate, and persists the verified expression pattern for later prioritisation.

## Safety and research boundaries

GDI is fail-closed. Ambiguous integration points, an unbuildable baseline, missing acceptance evidence, unsafe workspaces or an exhausted supported grammar produce a blocked result—not a speculative patch.

The current synthesizer is not a claim of universal program synthesis. Its verified scope is bounded integer-expression synthesis at identifiable return stubs with a working native test oracle. Extending that scope requires additional general synthesis grammars and semantic evidence, not project-specific rules.

## Build

Validated toolchain:

```text
Compiler: D:\android_project\windows\toolchain\bin\g++.exe  (GCC 16.1.0)
CMake:    C:\Program Files\CMake\bin\cmake.exe
Standard: C++20 with -Wall -Wextra -Wpedantic -Werror
```

```powershell
$env:PATH = 'D:\android_project\windows\toolchain\bin;' + $env:PATH

cmake -S . -B build -G 'MinGW Makefiles' `
  -DCMAKE_CXX_COMPILER='D:/android_project/windows/toolchain/bin/g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:/android_project/windows/toolchain/bin/mingw32-make.exe'
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

## Inductive synthesis workflow

```text
Protected project + existing native tests
  → isolated baseline build
  → discover eligible implementation stub
  → derive local variables and constants
  → create bounded candidate programs
  → compile + run the project’s own tests per candidate
  → retain only verified implementation and experience
```

Run it through the GDI executable:

```powershell
gdi_intelligence --foreign-project D:\path\to\project `
  --workspace D:\external-workspace `
  --inductive-synthesize
```

Optional persistence and budget controls:

```text
--inductive-model D:\models\gdi_inductive_experience.gdi
--inductive-max-candidates 50
```

The protected project is never the workspace. The command returns a non-zero result for `BLOCKED` or `FAILED`; generated evidence remains in the external workspace.

## Verification

`GDIInductiveSynthesisTests` uses two independent CMake projects with different function and parameter names. The first induces and verifies a previously stubbed arithmetic implementation. The second uses persisted experience to prioritise the learned expression family, while still compiling and running its own tests.

Project reports and reproducible campaign evidence are kept under [`GDI_VERIFICATION_REPORT`](GDI_VERIFICATION_REPORT).

## License

See [LICENSE](LICENSE).
