# GDI — Genomic Developmental Intelligence

**Release:** 1.5.0 · **Platform:** Windows x64 · **Core:** C++20 · **Status:** R17 implemented within a deliberately bounded, fail-closed synthesis scope.

GDI is a native C++20 system for analyzing, verifying, repairing and—within an explicitly supported scope—extending software projects. Its architecture combines an immutable synthetic genome, regulatory capability gates, build-contract reconstruction, strategy planning, isolated execution and persistent development evidence.

The current R17 release adds **Autonomous Software Engineering Intelligence (ASEI)**: a requirement can be transformed into a structured contract, a project-impact model, an architecture change plan, generated C++20 components, native build/test evidence and reusable development experience—without changing the protected source project.

> GDI does not claim universal autonomous programming. Unsupported, ambiguous or insufficiently evidenced requests are blocked instead of receiving speculative source changes.

## Highlights

- **R17 autonomous feature development** for supported CMake/C++20 tasks.
- **R15 BuildContract** discovery for CMake, MSBuild, Ninja, Make and direct compiler workflows.
- **R16 DevelopmentStrategy** planning, persistence, transfer scoring and fail-closed execution boundaries.
- **R17 RequirementContract**, ChangeImpactGraph and ArchitectureChangePlan with traceable evidence.
- **Isolated development workspaces** with original-tree integrity checks.
- **Native verification** through the discovered toolchain, linker and CTest rather than a substitute compiler path.
- **Persistent strategy and experience records** separate from the immutable genome.
- **Native Windows GUI** with R17 requirement-driven development mode.
- **KR integration evidence** for real KR, Rust, Go and SDL builds without modifying the KR toolchain.

## R17: Requirement to Verified Software Change

```text
Requirement
  → RequirementContract
  → R15 BuildContract + reproduced baseline
  → ChangeImpactGraph
  → ArchitectureChangePlan
  → R16 strategy graph
  → genome-authorized C++20 synthesis
  → isolated integration
  → native build, tests and acceptance evidence
  → persisted strategy and experience
```

R17 currently supports two verified capability classes:

| Capability | Generated behavior | Acceptance coverage |
| --- | --- | --- |
| `persistent_state` | Typed persistent string state with save/load and integrity rejection | write, restart/load, corrupt-data rejection |
| `integer_aggregation` | Integer-sequence total calculation | positive, empty and negative inputs |

Generated headers, implementations, acceptance tests and CMake integration are created only inside a new isolated workspace. The original project is checked before and after the run.

## Architecture

| Layer | Responsibility |
| --- | --- |
| Synthetic genome | Static capability definitions, grammar and regulatory loci; never mutated at runtime. |
| R15 BuildContract | Discovers targets, dependencies, compiler/generator data, SDK evidence and test topology. |
| R16 DevelopmentStrategy | Plans compatible operator graphs, records outcomes and persists reusable strategy evidence. |
| R17 ASEI | Interprets requirements, computes impact, selects architecture, synthesizes approved components and verifies acceptance criteria. |
| Native verification | Runs the real project build, linker, executable and CTest path in an isolated workspace. |
| Experience stores | Persist project facts, transferable principles, strategy graphs and R17 outcome records with provenance. |

## Quick Start

### Prerequisites

- Windows 11 x64
- CMake 3.20 or newer
- A C++20 compiler
- A supported generator (the validated reference uses **MinGW Makefiles** and GCC 16.1.0)

Reference toolchain paths used for the verified Windows build:

```text
D:\android_project\windows\toolchain\bin\g++.exe
D:\android_project\windows\toolchain\bin\mingw32-make.exe
```

### Build and Test

```powershell
$env:PATH = 'D:\android_project\windows\toolchain\bin;' + $env:PATH

cmake -S . -B build -G 'MinGW Makefiles' `
  -DCMAKE_CXX_COMPILER='D:/android_project/windows/toolchain/bin/g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:/android_project/windows/toolchain/bin/mingw32-make.exe'

cmake --build build -j 1
ctest --test-dir build --output-on-failure
```

The release baseline is compiled with:

```text
-std=c++20 -Wall -Wextra -Wpedantic -Werror
```

The R17 release verification completed **12/12 CTest tests** successfully.

## R17 CLI Usage

Use `gdi_intelligence` to develop a supported new capability in an isolated copy of a CMake project:

```powershell
.\build\gdi_intelligence.exe `
  --foreign-project .\examples\r17_neutral_project `
  --development-requirement `
    'Add persistent session state that is saved, restored after restart, and rejected when corrupted.' `
  --workspace D:\gdi-runs\session-development `
  --compiler D:\android_project\windows\toolchain\bin\g++.exe `
  --cmake 'C:\Program Files\CMake\bin\cmake.exe' `
  --ctest 'C:\Program Files\CMake\bin\ctest.exe' `
  --r17-execute
```

Successful output includes:

```text
R17_REQUIREMENT=<stable requirement id>
R17_CAPABILITY=<supported capability class>
R17_ARCHITECTURE=<architecture plan id>
R17_CHANGES=<generated artifacts>
R17_ACCEPTANCE=<verified criteria>
ORIGINAL_UNTOUCHED=YES
EXPERIENCE_PERSISTED=YES
RESULT=COMPLETE
```

Omit `--r17-execute` to inspect and persist the contract, impact and architecture plan without carrying out the generated native build. The result is then intentionally `INCOMPLETE`.

## Windows GUI

After a Windows build, start:

```powershell
.\build\gdi_gui.exe
```

`gpi_gui.exe` remains available for compatibility. Select:

```text
GDI R17 – Neue Softwarefunktion aus Anforderung entwickeln (isoliert)
```

1. Enter the new functional requirement in the upper task field.
2. Select the target project directory in the project field.
3. Choose **Ziel analysieren** to inspect the requirement, build contract, impact graph and architecture plan.
4. Choose **Entwicklung starten** to run the isolated R17 pipeline.

The GUI launches the same production backend as the CLI, displays process output and records the result workspace. Its self-test also exercises the R17 requirement pipeline:

```powershell
.\build\gdi_gui.exe --self-test
```

## Build-Contract and Repair Intelligence

R15/R16 capabilities remain available through `gdi_intelligence`:

```powershell
.\build\gdi_intelligence.exe `
  --foreign-project <project-root> `
  --workspace <isolated-workspace> `
  --execute-strategy `
  --strategy-memory <workspace>\strategies.gdis
```

GDI distinguishes source defects, configuration errors, unavailable SDKs, compiler-family incompatibilities, unresolved link dependencies and test/runtime failures. The native contract is reconstructed and executed before any successful outcome is reported.

## KR / Rust / Go / SDL Verification

The optional KR path regenerates `KR_BUILD` only in an isolated workspace, then validates real C++, Rust, Go and SDL integration plus a bounded runtime readiness marker.

```powershell
.\build\gdi_intelligence.exe --kr-e2e `
  --foreign-project F:\MDRAFTNE `
  --workspace D:\gdi-runs\mdraftne-e2e `
  --compiler D:\android_project\windows\toolchain\bin\g++.exe `
  --kr-java D:\android_project\windows\runtime\jdk\bin\java.exe `
  --kr-compiler-jar D:\android_project\windows\dist\KR-Compiler-IDE.jar `
  --runtime-ready-marker 'START: Erster Frame gerendert'
```

The KR compiler toolchain and protected original project are never modified.

## Security Model

- Foreign projects are imported read-only by default.
- Workspaces nested below a protected original project are rejected.
- Imports skip symbolic links/junctions and generated output directories.
- R17 writes only to explicitly generated paths in its isolated copy.
- A reproducible baseline is mandatory before source generation.
- Build, test and runtime outcomes are captured; a successful compile alone is insufficient.
- Original project hashes are compared before and after each development run.
- Unrecognized requirements, missing authorization and unsafe transformations fail closed.

## R18 Behavioral Evolution

R18 extends GDI from generating isolated components to safely integrating verified behavioral changes into an existing application's normal execution path. It reconstructs a native baseline, analyzes executable entry points and data/state relations, creates an isolated work copy, applies a digest-bound transformation, rebuilds the real application and verifies changed runtime behavior.

The release contains two independent end-to-end applications: validated CSV processing (skip and report malformed records) and state-aware CLI startup (resume valid state, reject inconsistent state). Unsupported requirements and ambiguous integration evidence fail closed without modifying the imported source tree.

The next research objective is deliberately broader than adding fixed behavioral classes: derive suitable integration points from the semantics of an unknown project itself, without a prepared `GDI_R18_HOOK` annotation. Until that semantic proof is available, R18 retains its explicit opt-in anchor as a safety boundary.

## Supported Scope and Explicit Limits

GDI is **not** a replacement for a complete C++20 compiler, linker, semantic theorem prover or general Rust/Go synthesis system.

The current R18 implementation does not claim:

- arbitrary natural-language requirement interpretation;
- arbitrary mutation of existing project sources or hook-free semantic integration;
- universal ISO C++20 semantic coverage;
- arbitrary cross-language implementation synthesis;
- automatic writeback to foreign original projects; or
- a completed independent blind-transfer research validation.

When a request is unsupported, ambiguous or insufficiently evidenced, GDI returns `BLOCKED` or `INCOMPLETE` instead of generating plausible-looking but unverified code.

## Verification Evidence

R18 implementation and verification records are stored in:

```text
GDI_VERIFICATION_REPORT/R18_IMPLEMENTATION/
├── SUMMARY.md
├── IMPLEMENTATION_MATRIX.md
├── SAFETY_AND_LIMITS.md
├── TEST_CAMPAIGN.md
└── IMPLEMENTATION_COMPLETION_REPORT.md
```

The public release and Windows-x64 package are available on the [GitHub Releases page](https://github.com/kruemmel-python/GDI-Genomic-Developmental-Intelligence/releases/tag/v1.6.0).

## Main Executables

| Executable | Role |
| --- | --- |
| `gdi_intelligence` | R15–R18 build, strategy, autonomous-development and behavioral-integration pipeline. |
| `gdi_gui` / `gpi_gui` | Native Windows interface, including R17 and R18 modes. |
| `gpi_sandbox` | Windows sandbox/process isolation support. |
| `gdi_create` | Bounded behavioral C++20 synthesis. |
| `gpi_architect` | Typed architecture-goal synthesis. |
| `gpi_synthesize` | Typed multi-file blueprint synthesis. |
| `gpi_develop` | Manifest-driven project development workflow. |
| `gpi_research` | Bounded research and hypothesis execution. |

## Documentation

- [Technical documentation](DOCUMENTATION.md)
- [R18 implementation summary](GDI_VERIFICATION_REPORT/R18_IMPLEMENTATION/SUMMARY.md)
- [R18 safety and limits](GDI_VERIFICATION_REPORT/R18_IMPLEMENTATION/SAFETY_AND_LIMITS.md)
- [R18 completion report](GDI_VERIFICATION_REPORT/R18_IMPLEMENTATION/IMPLEMENTATION_COMPLETION_REPORT.md)

## License

See [LICENSE](LICENSE).
