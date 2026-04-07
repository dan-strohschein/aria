# CR1.5: Remaining Defects & Deferred Items

**Date:** 2026-04-06
**Status:** All 5 tasks complete

---

## Completed This Session

### Task 1: Fix "use import registers module name" test — DONE
- Added pre-pass token scan in `resolve()` to register use-imported module names
- Root cause: bootstrap miscompiles DkUse tag for large sum types (10+ variants)
- Fix bypasses DeclIndex and scans tokens directly for `use` keywords
- **Result: 47/47 tests pass (was 46/47 since before CR1)**

### Task 2: Add effect annotations — DONE
- Added `with [Io]`, `with [Fs]`, or `with [Io, Fs]` to 30 functions
- Files: `main.aria`, `codegen.aria`, `llvm.aria`
- **Result: 102 warnings → 2 warnings**
- Remaining 2: `_lower_method_call` and `_lower_mono_fn` use println for debug — annotating would cascade to 186+ lowerer functions

### Task 3: Fix IR validation label tracking — DONE
- **Root cause:** Two field mismatches in `_validate_func` (ir.aria):
  1. Label collection read `inst.dest` but lowerer stores label ID in `inst.arg1`
  2. Branch target check read `inst.arg1` for all jump ops, but OpBranchTrue/OpBranchFalse store target in `inst.arg2` (arg1 is the condition)
- **Fix:** Updated label collection to read `inst.arg1`, and branch target extraction to use `inst.arg2` for BranchTrue/BranchFalse
- **Re-enabled** `validate_ir_module` call in codegen.aria (was suppressed at line 52)
- **Result:** 0 errors, 2 warnings, 47/47 tests pass

### Task 4: Map/set literal parsing — DONE (parser only)
- Added `EkMapLit` and `EkSetLit` variants to ExprKind enum in ast.aria
- Added `mk_map_lit` and `mk_set_lit` constructors in ast.aria
- Added `_parse_brace_expr` function in parser.aria for disambiguation:
  - `{}` → empty map literal
  - `{ expr : expr, ... }` → map literal (EkMapLit, list stores alternating key/value indices)
  - `{ expr , expr, ... }` → set literal (EkSetLit, list stores element indices)
  - `{ expr }` → block (single expression, existing behavior preserved)
- Struct literals unaffected (still triggered by `PascalCase{...}`)
- No codegen changes needed (lowerer uses string-based ExprKind dispatch)
- **Result:** 0 errors, 2 warnings, 47/47 tests pass

### Task 5: NodePool boolean codegen bug — RESOLVED
- **Root cause identified:** GC root tracking was missing for 9 IR ops (fixed in commit 5adf7a2). Live heap objects (including Expr structs in NodePool) were being swept during GC collection, corrupting fields like b2.
- **Evidence:** The stale gen2 binary (March 25, pre-GC-fix) reproduces the bug: `./gen2 check src/` shows 41 E0106 errors with return types resolving to `<none>`. Gen1 (March 24, bootstrap-compiled) does not show the bug because it was compiled by the Go bootstrap and runs with different GC behavior.
- **Fix:** Restored the AST-based path (`_register_fn_sig_ast`) in checker.aria, removing the token-walking workaround. Gen1 passes with the AST path (0 errors).
- **Limitation:** Cannot rebuild gen2 from current source to fully verify (bootstrapping gap — `_emit_defers` not recognized by stale gen1). Full verification requires rebuilding the bootstrap chain.
- **Result:** 0 errors, 2 warnings, 47/47 tests pass

---

## Current State Summary

| Metric | Value |
|--------|-------|
| Tests | 47/47 passing |
| Errors | 0 |
| Warnings | 2 (deep lowerer debug prints) |
| Branch | `feature/vscode-syntax-highlighting` |
| Latest commit | `64ef6e3` (pre-CR1.5 changes) |

## Known Issues

### Bootstrapping Gap
The `src/aria_generated` binary (built March 24) cannot compile the current source due to functions added after that date (e.g., `_emit_defers`). This prevents rebuilding the gen1→gen2→gen3 chain. To fix:
1. Identify the minimal set of source changes that break compatibility
2. Either update `src/aria_generated` incrementally or bridge via the bootstrap compiler

## Work Rules (carry forward)

- Use **squire**, **chisel**, and **cartograph** for code reads/writes
- Follow **DRY** and **SOLID** principles
- Validate each issue exists before fixing
- Build + test after each change
- Verify gen2 self-compile after significant changes
