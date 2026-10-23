# R19 test campaign

- **A – CSV multi-file:** discovers the input-loop-to-processor call chain, inserts validation before the processor call, preserves valid order and records malformed input.
- **B – stateful CLI:** discovers the entrypoint-to-state-reader transition and inserts corruption rejection before the existing resume call; valid state still reaches the normal application function.
- **C – negative:** an under-specified requirement is `BLOCKED`, with no developed copy and unchanged original hash.
- **D – transfer:** a differently named catalog/emitter application revalidates the same semantic preconditions without fixture-name logic.

All positive cases use CMake, native build contracts and separate normal executable processes.
