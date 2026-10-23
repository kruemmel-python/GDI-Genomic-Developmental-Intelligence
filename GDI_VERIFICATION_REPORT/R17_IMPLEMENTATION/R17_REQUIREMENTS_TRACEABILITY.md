# R17 Requirements Traceability

| Requirement | Implementation | Evidence |
| --- | --- | --- |
| Structured requirements | `RequirementContract`, persistence with integrity record | `test_r17_asei`; `evidence/smoke/unit/requirement.gdir17c` |
| Change impact | `ChangeImpactGraph` source/target nodes and edges | `test_r17_asei` |
| Architecture choice | `ArchitectureChangePlan` alternatives and selected component | both acceptance `r17_development_result.json` files |
| R15/R16 reuse | baseline via `executeNativeBuild`; plan via `AutonomousStrategyPlanner` | acceptance workspaces |
| Safe generation | isolated copy, generated directory, restricted capability classes | original unchanged result fields |
| Build and behavior | generated CTest targets execute persistent/write/restart/corruption and aggregation/empty/negative tests | acceptance result reports and native logs |
| Persistence | GDIR17C1, GDIR17E1 and GDIS1 strategy storage | acceptance workspace files |
| CLI and GUI | `--development-requirement`/`--r17-execute`, GUI R17 mode | CLI logs; GUI self-test JSON |
