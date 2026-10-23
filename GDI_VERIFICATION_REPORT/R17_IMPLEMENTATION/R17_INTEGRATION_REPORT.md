# R17 Integration Report

## CLI

```text
gdi_intelligence --foreign-project <project> \
  --development-requirement <requirement> --workspace <isolated-workspace> \
  --r17-execute --cmake <cmake> --ctest <ctest>
```

Without `--r17-execute`, the command performs contract, impact and architecture work in an isolated workspace but returns `INCOMPLETE`; no native generated build is reported as complete.

## GUI

Select **“GDI R17 – Neue Softwarefunktion aus Anforderung entwickeln (isoliert)”**. The upper task field accepts the requirement, and the specification field accepts the project directory. “Ziel analysieren” calls the same requirement/build/impact/architecture APIs as the CLI. “Entwicklung starten” invokes the R17 CLI path with `--r17-execute` in a job-contained process.

## Native integration facts

For each R17 run the executor:

1. rejects a workspace nested in the protected source project;
2. creates a symlink-skipping copy;
3. runs the R15 baseline before mutation;
4. writes generated sources only under `generated/` in that copy;
5. appends a CMake component library plus acceptance target;
6. runs R15 native configure/build/test on a further isolated build copy;
7. checks the original tree hash before/after; and
8. persists requirement and strategy/experience records.

The concrete command output, native build reports, generated source, logs and state records are retained below `evidence/acceptance/`.
