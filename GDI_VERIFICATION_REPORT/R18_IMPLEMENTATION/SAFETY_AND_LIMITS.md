# R18 safety and limits

R18 never writes back to the imported project. It rejects workspaces inside the original project using the existing R15 isolation validation, skips symlinks during copying, compares the original tree hash before and after, and applies no source change without a uniquely discovered `GDI_R18_HOOK` anchor and matching precondition.

This is not a claim of arbitrary semantic C++ repair. R18 currently supports two verified behavioral classes and requires an explicit, auditable integration anchor. Unsupported or ambiguous requests are reported as `BLOCKED`; they are not silently synthesized.
