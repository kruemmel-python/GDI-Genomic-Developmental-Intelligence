# GDI R18 – Autonomous Behavioral Evolution & Semantic Integration

R18 adds a fail-closed behavioral-evolution pipeline for existing C++20 applications. It first reconstructs the native baseline through the R15 build contract, analyzes entry points and explicit semantic integration anchors, applies digest-checked transformations only to an isolated copy, rebuilds the actual application, and invokes its normal executable entry point for behavioral acceptance tests.

Implemented behavioral classes are deliberately bounded and evidence-backed:

- validated record processing: valid CSV records remain in order; invalid records are skipped and reported;
- validated state startup: a valid persisted state resumes; malformed state is rejected by the application entry point.

Unsupported requirements, ambiguous integration anchors, failed baselines, and non-isolated workspaces fail closed. The source project hash is compared before and after every run.
