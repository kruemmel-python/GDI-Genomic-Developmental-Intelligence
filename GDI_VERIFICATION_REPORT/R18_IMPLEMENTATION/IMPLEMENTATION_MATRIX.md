# R18 implementation matrix

| Requirement | Component | Implementation and evidence |
|---|---|---|
| Behavioral contract | `autonomous_behavioral_evolution` | Typed contract and confidence-bounded recognition for record/state behavior. |
| Program and impact model | same | Entry point, source, data-flow, return-boundary and explicit integration-anchor extraction. |
| Existing source integration | same | Unique opt-in anchor plus before/after content digests; edits only in isolated copies. |
| Build integration | same | Generated component is linked to the discovered executable target and rebuilt through R15. |
| Runtime acceptance | same | Actual generated application executable is run with positive, negative and error-path inputs. |
| CLI / GUI | `gdi_intelligence`, `gpi_gui` | R18 analyze/plan/execute modes use the same backend. |
| Static genomic authorization | `genome_bootstrap.cpp` | Versioned design-time loci 16820–16827; runtime does not mutate genome data. |
| Persistence | R18 evidence | Report plus experience record are written in the isolated workspace. |
