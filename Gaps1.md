# Aria Spec vs Implementation — Gap Analysis

## Fully Implemented

- Basic types: `i64`, `f64`, `bool`, `str`, arrays, maps
- Functions with generics and trait bounds
- Structs and sum types with variants (tag-only, tuple, struct)
- Pattern matching (literals, variants, bindings, guards)
- Result types (`Ok`/`Err`, `?`, `!`, `catch`)
- Optional types (`Some`/`None`, `??`, `?.`)
- Closures with captures
- String interpolation and string methods
- For/while/loop with break/continue
- If/else if/else expressions
- Pipeline operator (`|>`)
- Compound assignment (`+=`, `-=`, etc.)
- Constants
- Defer (up to 4 per function)
- TCP networking, PostgreSQL, HTTP server, JSON library
- Basic concurrency: spawn, channels, mutexes
- Test blocks with test runner
- Multi-file compilation
- `derives` clause (parsed + registered)

---

## NOT Implemented — Organized by Priority

### Tier 1: Core Language Gaps (Would Block Real Programs) — ✅ ALL RESOLVED

| Feature | Spec Location | Status |
|---|---|---|
| **Exhaustive match checking** | `pattern-matching.md` | ✅ Implemented — E0400 error for missing variants; supports sum types, bool, Optional, Result; guards don't satisfy exhaustiveness; or-patterns tracked |
| **Mutability enforcement** | `design-decisions-v01.md` §5 | ✅ Implemented — E0500 error when reassigning immutable bindings (`name = expr` without `mut`) |
| **Effect system enforcement** | `effect-system.md` | ✅ Implemented — E0106 error for builtin `_aria*` functions (Fs, Io, Net, Async) called from pure functions; user-defined function effects were already enforced |
| **`pub` visibility enforcement** | `scoping-rules.md` | ✅ Implemented — E0704 error when accessing private symbols from a different file |
| **Import aliases** | `formal-grammar.md` | ✅ Implemented — `use mod as alias` registers alias as additional SkModule symbol |
| **Hex/octal/binary literal values** | `formal-grammar.md` §2 | ✅ Implemented — `0xFF`, `0o77`, `0b1010` correctly parsed with underscore separators |
| **`where` clause enforcement** | `generics-type-parameters.md` | ✅ Implemented — `where T: Trait` parsed and merged into generic bound slots; works for both top-level `fn` and `impl` methods |

### Tier 2: Type System Completeness — ✅ ALL RESOLVED

| Feature | Spec Location | Status |
|---|---|---|
| **Type aliases** (`alias Name = Type`) | `newtype-aliases.md` | ✅ Implemented — resolver registers alias as SkType, checker registers as TyNamed pointing to underlying type |
| **`dyn Trait` (trait objects)** | `trait-system.md` | ✅ Implemented — TyTraitObject type kind, `dyn Trait` type annotations, vtable construction via `_lower_make_trait_object`, `OpVtableCall` dispatch with method index lookup, `dyn Trait(expr)` explicit coercion syntax |
| **Associated types in traits** | `trait-system.md` | ✅ Implemented — `type Name` collected in trait bodies (stored in TraitDef.assoc_type_names), `type Name = Type` in impl bodies registers `TypeName_AssocName` alias, `Self::Assoc` / `Type::Assoc` resolved in type annotations |
| **Supertraits** (`trait B: A`) | `trait-system.md` | ✅ Implemented — parsed as `trait B: A, C`, stored in TraitDef.parent_traits, enforced at impl sites with E0200 |
| **`Convert`/`TryConvert` traits** | `type-conversions.md` | ✅ Implemented — formal Convert/TryConvert traits registered; `.to[T]()` and `.trunc[T]()` already work in codegen |
| **`T(x)` lossless widening syntax** | `type-conversions.md` | ✅ Implemented — primitive type names used as callables produce widening conversions in checker |
| **`char` type** | `string-handling.md` | ✅ Implemented — TkCharLit token, `'c'` lexing with escape sequences, TY_CHAR type, codegen as codepoint integer |
| **Smaller int types** (`i8`/`i16`/`i32`/`u*`) | `formal-grammar.md` | ✅ Already fully implemented — all 8 types with full trait support |
| **`Drop` trait / destructors** | `memory-management.md` | ✅ Implemented — Drop trait registered, `_emit_drops_scoped` emits `TypeName_drop(self)` calls at scope exit with per-scope tracking via scope_start index, integrated into `_lower_block` |
| **`with` resource blocks** (RAII) | `closures-capture-semantics.md` | ✅ Implemented — `with expr { body }` lowered: resource expression evaluated, body executed, `TypeName_drop(resource)` emitted at block exit; scoped drops handle early returns within the body |
| **Float `Eq`/`Hash` prohibition** | `equality-comparison.md` | ✅ Fixed — floats only implement Ord, Add, Sub, Mul, Div, Mod, Numeric; Eq and Hash removed |

### Tier 3: Pattern Matching Completeness

| Feature | Spec Location | Status |
|---|---|---|
| **Or-patterns** (`Pat1 \| Pat2 =>`) | `pattern-matching.md` | Not handled in lowerer |
| **Struct variant destructuring** (`Variant{field: v} =>`) | `pattern-matching.md` | Partial — bootstrap codegen can't do it |
| **Array/slice patterns** (`[first, ..rest]`) | `pattern-matching.md` | Not implemented |
| **Tuple patterns** (`(a, b)`) | `pattern-matching.md` | Not implemented (no tuple type) |
| **Named patterns** (`name @ Pattern`) | `pattern-matching.md` | Not implemented |
| **Refutable bindings** (`Pattern := expr else { }`) | `pattern-matching.md` | Not implemented |
| **`is` type check in guards** | `pattern-matching.md` | Not implemented |
| **Nested patterns** | `pattern-matching.md` | Only one level deep |

### Tier 4: Concurrency Model

| Feature | Spec Location | Status |
|---|---|---|
| **Structured `scope { }` blocks** | `concurrency-design.md` | Not implemented — no auto-join semantics |
| **`select` expression** | `concurrency-design.md` | Not implemented |
| **`Task[T]` methods** (`.done()`, `.cancel()`, `.result()`) | `concurrency-design.md` | Only `.await()` works |
| **`spawn.detach`** | `concurrency-design.md` | Not implemented |
| **Directional channels** (`chan.Send[T]`, `chan.Recv[T]`) | `concurrency-design.md` | Not implemented |
| **`for msg in channel { }`** | `concurrency-design.md` | Not implemented |
| **Cancellation tokens** | `concurrency-design.md` | Not implemented |
| **`RWMutex`, `Atomic`, `WaitGroup`, `Once`, `Barrier`** | `concurrency-design.md` | Not implemented |
| **`Send`/`Share` auto-derive + checking** | `concurrency-design.md` | Not implemented |

### Tier 5: String & Literal Completeness

| Feature | Spec Location | Status |
|---|---|---|
| **Raw strings** (`r"..."`, `r#"..."#`) | `string-handling.md` | Not lexed |
| **Triple-quoted strings** (`"""..."""`) | `string-handling.md` | Not lexed |
| **Char literals** (`'A'`, `'\n'`) | `string-handling.md` | Not lexed |
| **Duration literals** (`30s`, `5ms`) | `formal-grammar.md` | Not lexed |
| **Size literals** (`512kb`, `4mb`) | `memory-management.md` | Not lexed |
| **Format specifiers** (`{val:.4}`, `{val:#x}`) | `string-handling.md` | Not implemented |
| **StringBuilder** | `string-handling.md` | Not implemented |
| **`.charCount()`, `.chars()`, `.graphemes()`** | `string-handling.md` | Not implemented (byte-only) |

### Tier 6: Error Handling Completeness

| Feature | Spec Location | Status |
|---|---|---|
| **Error traces** (`ErrorTrace[E]`, `TraceFrame`) | `error-handling.md` | `?` propagates but no trace frames |
| **`? \|e\| transform`** (error transformation) | `error-handling.md` | Not implemented |
| **Typed `catch` arms** (`catch { Pat => expr }`) | `error-handling.md` | Only `catch \|e\| { }` form |
| **`or default_value`** sugar | `error-handling.md` | Not implemented |
| **`must "message"`** | `error-handling.md` | Not implemented |
| **`retry` built-in** | `error-handling.md` | Not implemented |
| **Error category traits** (`Transient`, `Permanent`, etc.) | `error-handling.md` | Not implemented |
| **Error union in signatures** (`! E1 \| E2`) | `error-handling.md` | Not implemented |

### Tier 7: Memory Management

| Feature | Spec Location | Status |
|---|---|---|
| **Garbage collector** | `garbage-collector.md` | No GC — malloc only, no free (leaks by design) |
| **`@stack` annotation** | `memory-management.md` | Not implemented |
| **`@arena` annotation** | `memory-management.md` | Not implemented |
| **`@inline` field annotation** | `memory-management.md` | Not implemented |
| **`Pool[T]`** | `memory-management.md` | Not implemented |
| **Move semantics / ownership tracking** | `memory-management.md` | Not implemented |

### Tier 8: Compiler Infrastructure

| Feature | Spec Location | Status |
|---|---|---|
| **`aria fix` command** | `compiler-diagnostics.md` | Not implemented |
| **`aria explain E0042`** | `compiler-diagnostics.md` | Not implemented |
| **JSON diagnostic output** (`--format=json`) | `compiler-diagnostics.md` | Not implemented |
| **Multi-span diagnostics** | `compiler-diagnostics.md` | Single-span only |
| **"Did you mean?" suggestions** | `compiler-diagnostics.md` | Not implemented |
| **DWARF debug info** | `compiler-architecture.md` | Not emitted |
| **Cross-compilation** | `compiler-architecture.md` | Hardcoded to `arm64-apple-macosx14.0.0` |
| **`bench` blocks** | `testing-framework.md` | Not implemented |
| **Property-based testing** (`forAll`) | `testing-framework.md` | Not implemented |
| **Test fixtures** (`before`/`after`) | `testing-framework.md` | Not implemented |
| **Mocking** (`mock path with fn { }`) | `testing-framework.md` | Not implemented |

### Tier 9: Not Needed for Bootstrap (Deferred by Spec)

| Feature | Spec Location | Status |
|---|---|---|
| **FFI** (`extern "C" fn`) | `effect-system.md` | Explicitly deferred |
| **Annotations** (`@deprecated`, `@cold`, `@os`) | `annotations.md` | Not implemented |
| **`comptime fn`** | `const-evaluation.md` | Explicitly deferred to post-v0.1 |
| **Const generics** | `const-evaluation.md` | Explicitly deferred |
| **Partial application** | `closures-capture-semantics.md` | Explicitly deferred to v0.2 |
| **LSP server** | `compiler-architecture.md` | Planned, not implemented |
| **PGO / LTO** | `compiler-architecture.md` | Planned, not implemented |
