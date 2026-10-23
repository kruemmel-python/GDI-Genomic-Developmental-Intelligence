# GDI R17 — Autonomous Software Engineering Intelligence

Status: IMPLEMENTED for the declared, bounded R17 C++20 synthesis scope. This is an implementation result, not a claim of universal autonomous software engineering or a completed R17 research validation.

## Product path

`--development-requirement` in `gdi_intelligence` now drives this real pipeline:

1. Requirement text is interpreted into `RequirementContract` (`GDIR17C1`).
2. R15 discovers and reproduces the native build contract before source mutation.
3. `ChangeImpactGraph` is built from targets, sources and generated component boundaries.
4. `ArchitectureChangePlan` selects a minimally invasive, verifiable component design.
5. R16 plans a compatible strategy using the current build contract.
6. Genomically registered R17 operations generate a header, implementation, acceptance executable and CMake integration only in an isolated copy.
7. R15 executes the native CMake build and CTest suite against the developed copy.
8. Acceptance evidence, requirement contract and strategy/experience are persisted.

## Delivered components

- `include/gpi/autonomous_software_engineering.hpp`
- `src/gpi/autonomous_software_engineering.cpp`
- CLI integration in `apps/gdi_intelligence.cpp`
- GUI R17 mode in `apps/gpi_gui.cpp`
- Static R17 operator loci in `src/genome/genome_bootstrap.cpp`
- Neutral integration project and `test_r17_asei`.

## Executed evidence

- Clean full build: `evidence/build/full_build.log`.
- Final full regression: `evidence/smoke/ctest_final.log` — 12/12 passed in 745.18 seconds.
- Persistent-state extension: `evidence/acceptance/persistent_session/r17_development_result.json` — COMPLETE, 4 generated changes, 3 verified acceptance criteria, original unchanged.
- Integer aggregation extension: `evidence/acceptance/integer_aggregation/r17_development_result.json` — COMPLETE, 4 generated changes, 3 verified acceptance criteria, original unchanged.
- GUI backend path: `evidence/smoke/gdi_gui_self_test_final.json`.

The generated component name derives from the requirement capability class, not from a project or fixture name. The same architecture pipeline produced two distinct component types.
