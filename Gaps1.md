# Aria Spec vs Implementation — Gap Analysis

## Fully Implemented

- Basic types: `i64`, `f64`, `bool`, `str`, arrays, maps
- Functions with generics and trait bounds (including multi-bounds `T: Eq + Hash`)
- Structs and sum types with variants (tag-only, tuple, struct)
- Pattern matching (literals, variants, bindings, guards, or-patterns, named `@`, nested, tuple, array/slice, float, refutable bindings)
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
- Self-compilation: compiler can compile its own source to a native binary
- Directory expansion: `build src/` recursively finds `.aria` files
- `derives` clause with codegen (Eq, Clone, Debug generate actual method bodies)
- `where` clauses with multi-bound support
- `dyn Trait` with vtable dispatch
- `Drop` trait with per-scope LIFO destruction
- `with` resource blocks (RAII)
- Send/Share auto-derive for primitives
- `parseFloat` support (`.to[f64]()` on strings)

---

## NOT Implemented — Organized by Priority

### Tier 1: Core Language Gaps (Would Block Real Programs) — ✅ ALL RESOLVED

| Feature | Spec Location | Status |
|---|---|---|
| **Exhaustive match checking** | `pattern-matching.md` | ✅ Implemented — E0400 error for missing variants; supports sum types, bool, Optional, Result; guards don't satisfy exhaustiveness; or-patterns tracked |
| **Mutability enforcement** | `design-decisions-v01.md` §5 | ✅ Implemented — E0500 error when reassigning immutable bindings (`name = expr` without `mut`) |
| **Effect system enforcement** | `effect-system.md` | ✅ Implemented — warnings for builtin `_aria*` functions (Fs, Io, Net, Async) called from pure functions; user-defined function effects enforced |
| **`pub` visibility enforcement** | `scoping-rules.md` | ✅ Implemented — E0704 error when accessing private symbols from a different file |
| **Import aliases** | `formal-grammar.md` | ✅ Implemented — `use mod as alias` registers alias as additional SkModule symbol |
| **Hex/octal/binary literal values** | `formal-grammar.md` §2 | ✅ Implemented — `0xFF`, `0o77`, `0b1010` correctly parsed with underscore separators |
| **`where` clause enforcement** | `generics-type-parameters.md` | ✅ Implemented — `where T: Trait + Trait2` parsed and enforced with multi-bound checking |

### Tier 2: Type System Completeness — ✅ CORE RESOLVED (advanced features deferred to v0.2)

| Feature | Spec Location | Status |
|---|---|---|
| **Type aliases** (`alias Name = Type`) | `newtype-aliases.md` | ✅ Implemented |
| **`dyn Trait` (trait objects)** | `trait-system.md` | ✅ Implemented — vtable construction, `OpVtableCall` dispatch, `dyn Trait(expr)` coercion, object safety validation |
| **Associated types in traits** | `trait-system.md` | ✅ Implemented — `Type::Assoc` resolution works |
| **Supertraits** (`trait B: A`) | `trait-system.md` | ✅ Implemented — enforced at impl sites with E0200 |
| **`Convert`/`TryConvert` traits** | `type-conversions.md` | ✅ Implemented — `.to[T]()`, `.trunc[T]()`, `T(x)` widening |
| **`char` type** | `string-handling.md` | ✅ Implemented |
| **`Drop` trait / destructors** | `memory-management.md` | ✅ Implemented — per-scope LIFO drops |
| **`with` resource blocks** (RAII) | `closures-capture-semantics.md` | ✅ Implemented |
| **Float `Eq`/`Hash` prohibition** | `equality-comparison.md` | ✅ Implemented |
| **Multi-bound generics** (`T: Eq + Hash`) | `generics-type-parameters.md` | ✅ Implemented — parsed in generic params and where clauses, each bound checked independently |
| **`derives` codegen** | `trait-system.md` | ✅ Implemented — `derives [Eq, Clone, Debug]` generates actual method bodies (field-by-field eq, struct copy clone, string repr debug) |
| **Struct `==`/`!=` comparison** | `equality-comparison.md` | ✅ Implemented — `_lower_struct_eq` does field-by-field comparison |
| **Send/Share marker traits** | `concurrency-design.md` | ✅ Implemented — auto-derived for all primitive types |
| **`parseFloat`** | `type-conversions.md` | ✅ Implemented — `.to[f64]()` on strings via `_aria_str_to_float` runtime |

#### Tier 2 — Deferred to v0.2

| Feature | Spec Location | Reason for Deferral |
|---|---|---|
| **Blanket implementations** (`impl[T: Display] Describe for T`) | `trait-system.md` | Complex dispatch logic. No compiler source or typical user programs need this yet. Requires significant changes to trait resolution. |
| **Orphan rules** (impl conflict checking) | `trait-system.md` | Only relevant when multiple crates/packages exist. Single-crate compiler doesn't need this. Will matter when package system is added. |
| **Generic traits** (`trait Container[T]`) | `trait-system.md` | TraitDef has no generic parameter support. Would require extending trait registry, impl matching, and monomorphization. No compiler usage. |
| **Phantom types** | `generics-type-parameters.md` | Exotic feature for zero-cost type-level markers. No practical usage in the compiler or typical programs. |
| **Conditional impls** (`impl[T: Eq] Eq for Stack[T]`) | `generics-type-parameters.md` | Requires type-level conditional logic at monomorphization time. No compiler usage. Derives cover the common case. |
| **User-defined Convert/TryConvert** | `type-conversions.md` | Traits are registered but not connected to `T(x)` / `.to[T]()` syntax for user types. Built-in primitive conversions cover current needs. |
| **ConversionError sum type** | `type-conversions.md` | Error type for `.to[T]()` is hardcoded to str. Define when TryConvert is actually used by programs. |
| **Move semantics / use-after-move** | `memory-management.md` | Major effort (linear types analysis). Aria uses GC by default, so memory safety doesn't depend on this. Would require whole-program dataflow analysis. |
| **`ref` capture in closures** (`fn[ref x]()`) | `closures-capture-semantics.md` | Default value capture works for all compiler needs. Ref capture is an optimization for large values. |
| **`once` closures** | `closures-capture-semantics.md` | Niche optimization for single-use closures. No compiler or typical program usage. |
| **Method references** (`Type.method` as value) | `closures-capture-semantics.md` | Sugar for `fn(x) => x.method()`. Explicit closures work as a workaround. |
| **Field shorthand** (`.name` for `fn(x) => x.name`) | `closures-capture-semantics.md` | Sugar. Full field access works. |
| **`ref self` / `mut ref self`** | `trait-system.md` | Aria's immutable-struct-with-copy semantics means `self` by value covers all needs. Would require borrow checker. |
| **Default method bodies in traits** | `trait-system.md` | Infrastructure added (TraitDef has default_starts/default_ends fields). Body synthesis deferred — requires token replay. |
| **Hash+Eq enforcement on Map keys** | `equality-comparison.md` | Map is used as a built-in. No type-level key constraint checking yet. Low impact — wrong key types would just produce wrong hash results. |
| **Return-context type inference** | `generics-type-parameters.md` | Forward inference (argument→return) works. Reverse inference (return→argument) not implemented. Workaround: explicit types. |
| **Operator desugaring to trait calls** | `equality-comparison.md` | `==`/`<` on structs already use field-by-field comparison. Full desugaring to `TypeName_eq()` method calls works for derived types. Not yet dispatched through trait vtables for arbitrary user types. |

### Tier 3: Pattern Matching Completeness — ✅ ALL RESOLVED

| Feature | Spec Location | Status |
|---|---|---|
| **Or-patterns** (`Pat1 \| Pat2 =>`) | `pattern-matching.md` | ✅ Implemented |
| **Struct variant destructuring** (`Variant{field: v} =>`) | `pattern-matching.md` | ✅ Implemented — field name lookup via `variant_field_name` |
| **Array/slice patterns** (`[first, ..rest]`) | `pattern-matching.md` | ✅ Implemented — `OpArraySlice` opcode + runtime `_aria_array_slice` |
| **Tuple patterns** (`(a, b)`) | `pattern-matching.md` | ✅ Implemented — positional destructuring with literal sub-elements |
| **Named patterns** (`name @ Pattern`) | `pattern-matching.md` | ✅ Implemented — binds whole subject + sub-pattern |
| **Refutable bindings** (`Pattern := expr else { }`) | `pattern-matching.md` | ✅ Implemented — variant tag check + else block |
| **`is` type check in guards** | `pattern-matching.md` | Skipped — reserved for future versions per spec |
| **Nested patterns** (`Some(Circle(r))`) | `pattern-matching.md` | ✅ Implemented — one level of nesting |
| **Float literal patterns** | `pattern-matching.md` | ✅ Implemented — works through bootstrap; LLVM path pending lexer fix |

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

### Tier 5: String & Literal Completeness

| Feature | Spec Location | Status |
|---|---|---|
| **Raw strings** (`r"..."`, `r#"..."#`) | `string-handling.md` | Not lexed |
| **Triple-quoted strings** (`"""..."""`) | `string-handling.md` | Not lexed |
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

### Tier 8: Compiler Infrastructure

| Feature | Spec Location | Status |
|---|---|---|
| **`aria fix` command** | `compiler-diagnostics.md` | Not implemented |
| **`aria explain E0042`** | `compiler-diagnostics.md` | Not implemented |
| **JSON diagnostic output** (`--format=json`) | `compiler-diagnostics.md` | ✅ Implemented |
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
