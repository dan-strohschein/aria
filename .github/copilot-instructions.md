# Aria Language — GitHub Copilot Instructions

This repository contains the Aria programming language compiler. When generating or completing Aria code (`.aria` files), follow these rules:

## Syntax

- No semicolons. Newlines terminate statements.
- No parentheses around `if`/`while` conditions: `if x > 0 { }` not `if (x > 0) { }`
- Variable binding: `x := 42` (immutable), `mut x := 0` (mutable)
- Expression-body functions: `fn add(a: i64, b: i64) -> i64 = a + b`
- Block-body functions: `fn process(x: i64) -> i64 { ... }`
- Last expression is the implicit return value.
- String interpolation: `"Hello, {name}!"` (use `\{` to escape)
- Entry point: `entry { ... }` (not `fn main`)

## Types

- Default integer: `i64`. Default float: `f64`.
- Strings: `str`. Booleans: `bool`.
- Arrays: `[i64]`, `[str]`.
- No null. Use `Option[T]` or `T?` (sugar). Values: `Some(x)`, `None`.
- No exceptions. Use `Result[T, E]` or `T ! E` (sugar). Values: `Ok(x)`, `Err(e)`.
- Sum types: `type Shape = | Circle(f64) | Rect(f64, f64) | Point`
- Structs: `struct Point { x: f64, y: f64 }`

## Error Handling

- `?` propagates errors: `val := try_parse(input)?`
- `catch |e| { recovery }` for inline recovery
- `or default` for simple fallback: `val := parse(s) or 0`
- `must "msg"` panics on error: `val := required() must "needed"`
- `!` assert-unwrap: `val := parse("42")!`

## Functions & Generics

- `fn name[T: Bound](param: T) -> T`
- Where clauses: `fn f[T](x: T) where T: Eq + Hash`
- Effects: `fn read() -> str with [Io, Fs]`
- Closures: `fn(x: i64) -> i64 = x * 2`

## Pattern Matching

- `match val { Pattern => expr, ... }` — must be exhaustive
- Destructuring: `Circle(r) => ...`, `Some(x) => ...`
- Guards: `x if x > 0 => ...`
- Wildcard: `_ => ...`
- Or-patterns: `Red | Blue => ...`

## Traits & Impl

- `trait Display { fn display(self) -> str }`
- `impl Display for Point { fn display(self) -> str = "..." }`
- `derives [Eq, Clone, Debug]` on structs

## Concurrency

- `spawn expr` — spawn a task
- `scope { spawn a(); spawn b() }` — structured concurrency
- Channels: `ch.send(val)`, `ch.recv()`, `for msg in ch { }`

## FFI

- `extern fn abs(n: i64) -> i64`
- `extern "lib" { fn name(...) -> ret }`

## Modules

- First line: `mod myapp`
- Imports: `use std.{fs, json}`
- Public: `pub fn`, `pub struct`

## Testing

- `test "name" { assert expr == expected }`

## Do NOT

- Use semicolons
- Use parentheses around conditions
- Use `let`, `var`, `const` for bindings (use `:=` and `mut`)
- Return null (use `None` or `Err`)
- Use `class` or inheritance (use `struct` + `trait`)
- Use `try/catch` blocks (use `?`, `catch`, `or`)
