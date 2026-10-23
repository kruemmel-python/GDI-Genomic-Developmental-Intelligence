# R18 test campaign

`GDIR18BehavioralEvolutionTests` executes two independent CMake applications in isolated workspaces:

1. `r18_csv_application`: a normal record-processing executable is transformed so malformed rows are omitted from stdout and placed in an error report.
2. `r18_state_application`: a normal CLI entry point resumes `state:42` and rejects malformed state with a non-zero exit code.

The test additionally submits an unsupported behavioral requirement and verifies that no developed project is created. Each successful case verifies a reproduced baseline, transformed rebuild, normal executable invocation, original-tree integrity and persistent evidence.
