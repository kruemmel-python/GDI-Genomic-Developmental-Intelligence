# R17 Architecture

The R17 implementation extends the R15/R16 core rather than creating a parallel system.

| Stage | Product component | Safety / evidence boundary |
| --- | --- | --- |
| Requirement | `RequirementContract` | Unsupported or ambiguous classes are `BLOCKED`. |
| Discovery | `BuildContract` | R15 baseline must reproduce before mutation. |
| Impact | `ChangeImpactGraph` | Existing/generated/external paths are distinguished. |
| Design | `ArchitectureChangePlan` | Alternatives are scored; the selected plan names every artifact and build mutation. |
| Planning | `AutonomousStrategyPlanner` | Current R15 contract binds the R16 plan. |
| Synthesis | R17 authorized component generator | Only the declared persistence and integer-aggregation classes are emitted. |
| Integration | isolated project copy | No writeback path exists in the R17 CLI flow. |
| Verification | native CMake + CTest | Generated acceptance executable and prior native tests must pass. |
| Learning | GDIR17E1 + GDIS1 | Requirement/architecture/status and strategy are persisted separately from genome data. |

The executor is deliberately fail-closed: direct modification of existing application source is not attempted; CMake projects without a reproducible baseline or outside the supported synthesis classes terminate `BLOCKED`.
