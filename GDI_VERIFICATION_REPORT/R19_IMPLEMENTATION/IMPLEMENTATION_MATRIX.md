# R19 implementation matrix

| Requirement | Code path | Evidence |
|---|---|---|
| Semantic program model | `autonomous_semantic_integration` | Functions, entrypoints, calls, source digests, input/output/state flags. |
| Behavioral delta | same | Record loop and state-transition paths are derived from model relations. |
| Hook-free point discovery | same | Candidate is selected by data/control-flow preconditions, not comments or fixture names. |
| Safe transformation | same | Isolated copy, source/context digests, generated component, target linkage and native rebuild. |
| Runtime proof | same | Actual target executable runs with valid and invalid inputs. |
| CLI / GUI | `gdi_intelligence`, `gpi_gui` | R19 CLI contract and GUI backend self-test. |
| Genomic authorization | `genome_bootstrap.cpp` | Loci 16828–16834, design-time only. |
