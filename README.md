# GDI — Genomic Developmental Intelligence

**Release 1.7.0 · Windows x64 · C++20 · fail-closed autonomous software engineering research platform**

GDI is a native C++20 system for investigating how software projects can be discovered, analysed, changed and verified with explicit safety boundaries. It combines an immutable synthetic genome with build-contract reconstruction, semantic program modelling, isolated transformation, native compiler/linker validation and persistent, provenance-bound development evidence.

GDI reports success only when the real project build and the declared acceptance evidence succeed. Ambiguous, unsupported or insufficiently evidenced requests are blocked instead of receiving a speculative patch.

> Scope matters. GDI 1.7.0 proves bounded capabilities on real toolchains; it does not claim universal understanding or repair of arbitrary software.

## What is in this release

| Capability | What it does | Safety boundary |
| --- | --- | --- |
| R15 BuildContract | Reconstructs CMake, MSBuild, Ninja, Make and direct build contracts; identifies targets, compiler, SDK and tests. | Native baselines run only in isolated workspaces. |
| R16 DevelopmentStrategy | Selects and persists authorised development operator graphs with negative learning. | Incompatible or cyclic strategies are rejected. |
| R17 ASEI | Converts supported requirements into impact/architecture plans and isolated C++ components. | Only verified capability classes may be synthesised. |
| R18 Behavioural evolution | Analyses bounded behavioural change contracts and validates them through native execution. | Original projects are hash-checked before and after. |
| R19 semantic integration | Builds a source/call-flow model, discovers evidence-backed integration points and verifies generated changes end-to-end. | An unresolved or non-unique candidate yields `BLOCKED`. |
| Language-neutral requirement concepts | Maps German and English requirement expressions onto the same canonical concepts before source analysis. | This is a versioned language-pack layer, not a claim of universal natural-language understanding. |

## R19 workflow

```text
Requirement text
  → canonical semantic concepts
  → R15 build contract + protected baseline
  → semantic program / call-flow model
  → behavioural delta and candidate ranking
  → genome-authorised strategy
  → isolated source transformation
  → native build, linker, CTest and runtime acceptance
  → integrity proof and persistent evidence
```

The current verified R19 transformation classes are deliberately narrow:

| Capability | Required evidence | Runtime proof |
| --- | --- | --- |
| `record_validation` | Input loop, normal processor path and dedicated error sink | Valid records retained; invalid records excluded and reported. |
| `state_resume_validation` | State loader and a resolved entrypoint-to-process transition | Valid state starts normally; invalid state is rejected. |

German and English requests such as `ungueltige Datensaetze protokollieren` / `report invalid records` and `beschaedigten Zustand ablehnen` / `reject corrupt state` are projected onto these same concepts. The project code is analysed independently of the human wording.

## Proven verification and blind-transfer results

The repository contains reproducible positive fixtures for the bounded R19 classes. The independent transfer campaigns also document correct blocking rather than artificial success:

* **Chroniken der Gegenwelt:** native C++17 baseline built and tested; a crafting-material requirement was blocked because no authorised crafting/inventory transformation exists.
* **TATARUS CIVITAS:** native C++20 baseline built, CTest passed and the real CLI reference run completed; the original R19 run blocked. After the 1.7.0 language-concept extension, the German state request is recognised as `state_resume_validation`, but is still blocked because the project does not expose R19's proven state-loader-to-process pattern.

These results are evidence of the boundaries as well as of the capabilities. They are not presented as successful autonomous changes.

## Safety model

* Foreign project roots are treated as protected inputs.
* Work happens in external, isolated copies only.
* SHA-256 manifests establish original-tree integrity before and after a run.
* Path validation rejects unsafe workspace placement and traversal.
* Symlink/junction-sensitive import and copy logic is fail-closed.
* Generated build artefacts are protected; writeback requires explicit authorisation.
* Transformations are provenance- and digest-bound, then recompiled and re-tested.

## Build on Windows

Validated reference toolchain:

```text
Compiler: D:\android_project\windows\toolchain\bin\g++.exe  (GCC 16.1.0)
Build tool: D:\android_project\windows\toolchain\bin\mingw32-make.exe
CMake: C:\Program Files\CMake\bin\cmake.exe
```

```powershell
$env:PATH = 'D:\android_project\windows\toolchain\bin;' + $env:PATH

cmake -S . -B build -G 'MinGW Makefiles' `
  -DCMAKE_CXX_COMPILER='D:/android_project/windows/toolchain/bin/g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:/android_project/windows/toolchain/bin/mingw32-make.exe'

cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

Core targets are compiled with:

```text
-std=c++20 -Wall -Wextra -Wpedantic -Werror
```

## R19 CLI

Use the native R19 path against a protected project and an external workspace:

```powershell
.\build\gdi_intelligence.exe `
  --foreign-project D:\path\to\protected-project `
  --workspace D:\gdi-runs\semantic-integration `
  --semantic-integration-requirement `
    'Einen gespeicherten Zustand wiederherstellen und beschaedigte Zustandsdaten ablehnen.' `
  --cmake 'C:\Program Files\CMake\bin\cmake.exe' `
  --ctest 'C:\Program Files\CMake\bin\ctest.exe' `
  --r19-execute
```

Important output fields:

```text
R19_CAPABILITY=<canonical capability or empty>
R19_SELECTED_POINT=<source and symbol only when proven>
R19_TRANSFORMATIONS=<count>
R19_RUNTIME_TESTS=<count>
ORIGINAL_UNTOUCHED=YES|NO
RESULT=COMPLETE|INCOMPLETE|BLOCKED|FAILED
```

`BLOCKED` is a valid safety result. It means that GDI did not have sufficient general, authorised evidence to modify the project.

## Architecture and evidence

| Layer | Responsibility |
| --- | --- |
| Immutable genome | Static loci define capabilities and regulatory authority; the genome is never mutated at runtime. |
| Semantic engine | Models functions, source provenance, direct calls, input/output/state hints and uncertainty. |
| Development memory | Stores verified strategy outcomes and evidence independently of the genome. |
| Native verifier | Uses the discovered project toolchain and actual executable, not a substitute compiler path. |
| Windows GUI | Displays the current engine build identity and executes the integrated verification backend. |

Detailed implementation and test evidence is maintained in [GDI_VERIFICATION_REPORT](GDI_VERIFICATION_REPORT), including [R19 language semantics](GDI_VERIFICATION_REPORT/R19_IMPLEMENTATION/LANGUAGE_SEMANTICS.md).

## Repository layout

```text
apps/                         Native CLI and Windows GUI entry points
include/gpi/                  Public project-intelligence interfaces
src/gpi/                      Build contracts, strategies, semantic integration
src/genome/                   Immutable genomic capability definitions
tests/                        Native regression and end-to-end tests
examples/r19_*_semantic/      Hook-free R19 fixtures
GDI_VERIFICATION_REPORT/      Verification reports and reproducible evidence guidance
```

## License

See [LICENSE](LICENSE).
