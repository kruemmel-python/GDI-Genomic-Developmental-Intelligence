# R19 semantic integration architecture

R19 uses the R15 native build contract as its project boundary and baseline gate. `SemanticProgramModel` lexes supported C++20 sources into stable, digest-bound function and call nodes across translation units. `BehavioralDelta` binds a supported requirement to an input/state-to-output path. `SemanticIntegrationPlan` selects exactly one candidate only when its hard data-flow and control-flow preconditions are met.

The executor copies the protected project into an isolated workspace, applies a context-digest-bound source edit, adds a generated validation component, rebuilds the discovered executable target through R15 and invokes that same normal executable for acceptance. R18 remains unchanged as the explicit-hook compatibility path.

The supported R19 subset is deliberately bounded: stream-driven record loops with an in-scope error sink, and entrypoint-owned optional state transitions to a resolved application call. Dynamic dispatch, unresolved function calls, macros at the selected span, multiple equivalent candidates and unsupported contracts fail closed.
