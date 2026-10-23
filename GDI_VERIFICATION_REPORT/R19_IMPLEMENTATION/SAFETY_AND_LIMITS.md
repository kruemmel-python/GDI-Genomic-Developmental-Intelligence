# R19 safety and limits

R19 never edits an imported original. It validates workspace separation, copies without symlinks, records before/after original tree hashes and blocks a mutation unless there is one resolved candidate with all hard conditions satisfied.

This is an implemented semantic-integration subset, not a universal C++20 proof system. It does not authorize unresolved dispatch, macros at mutation spans, ambiguous candidate selection, arbitrary ownership transformations, templates beyond the supported local representation, threads or external ABI paths. Test-based runtime evidence proves exercised cases only.
