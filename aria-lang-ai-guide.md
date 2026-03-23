# Aria Programming Language — AI Assistant Guide

> Paste this document into any AI assistant's context to enable Aria code generation.
> Works with ChatGPT, Claude, Gemini, Copilot, and any LLM.

## What is Aria?

Aria is a statically typed, compiled programming language designed for clarity, safety, and performance. It compiles to native code via LLVM. Think of it as a blend of Rust's safety, Go's simplicity, and Swift's expressiveness — but with its own distinct design.

## The 5 Design Pillars

1. **Every token carries meaning** — no boilerplate, no ceremony
2. **The type system is the AI's pair programmer** — sum types, exhaustive matching, effects
3. **Compilation is instantaneous** — unambiguous grammar, minimal lookahead
4. **Performance is opt-in granular** — GC default, manual per-block
5. **No implicit behavior ever** — no implicit conversions, no hidden exceptions, no null

## Critical Syntax Rules

- **No semicolons** — newline terminates statements
- **No parentheses around conditions** — `if x > 0 { }` not `if (x > 0) { }`
- **No null** — use `Option[T]` (sugar: `T?`)
- **No unchecked exceptions** — use `Result[T, E]` (sugar: `T ! E`)
- **No inheritance** — use composition and traits
- **`mut` is on bindings, not types** — `mut x := Foo{}` makes `x` reassignable
- **Last expression is the return value** — no `return` needed for final expression
- **`=` for expression-body functions** — `fn add(a: i64, b: i64) -> i64 = a + b`

---

## Quick Reference

### Variables and Constants

```aria
// Immutable (default)
x := 42
name := "Aria"
pi := 3.14159

// Mutable
mut count := 0
count = count + 1

// Type annotation
age: i64 = 25
ratio: f64 = 0.75

// Constants
const MAX_SIZE: i64 = 1024
const VERSION: str = "1.0.0"
```

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `i64` | 64-bit signed integer (default) | `42` |
| `i32`, `i16`, `i8` | Smaller signed integers | `i32(100)` |
| `u64`, `u32`, `u16`, `u8` | Unsigned integers | `u8(255)` |
| `f64` | 64-bit float (default) | `3.14` |
| `f32` | 32-bit float | `f32(1.5)` |
| `bool` | Boolean | `true`, `false` |
| `str` | UTF-8 string | `"hello"` |
| `char` | Unicode codepoint | `'A'` |

### Number Literals

```aria
decimal := 1_000_000
hex := 0xFF
octal := 0o77
binary := 0b1010_1010
float := 3.14e-2
duration := 30s         // 30 seconds in nanoseconds
size := 4kb             // 4096 bytes
```

### Strings

```aria
// Basic
greeting := "Hello, world!"

// Interpolation
name := "Aria"
msg := "Hello, {name}!"
expr := "Result: {x * 2 + 1}"

// Raw strings (no escape processing)
path := r"C:\Users\name\file.txt"
regex := r#"(\d+)-(\d+)"#

// Multi-line
text := """
    First line
    Second line
    """

// String methods
s := "Hello, World"
s.len()                  // byte length
s.charCount()            // codepoint count
s.charAt(0)              // "H"
s.substring(0, 5)        // "Hello"
s.contains("World")      // true
s.startsWith("Hello")    // true
s.endsWith("World")      // true
s.indexOf("World")       // 7
s.trim()                 // remove whitespace
s.toLower()              // "hello, world"
s.toUpper()              // "HELLO, WORLD"
s.replace("World", "Aria") // "Hello, Aria"
s.split(", ")            // ["Hello", "World"]
```

### Functions

```aria
// Block body
fn greet(name: str) -> str {
    "Hello, {name}!"
}

// Expression body (single expression)
fn add(a: i64, b: i64) -> i64 = a + b
fn square(x: f64) -> f64 = x * x
fn is_even(n: i64) -> bool = n % 2 == 0

// No return value
fn log(msg: str) {
    println("[LOG] {msg}")
}

// With effects
fn read_config(path: str) -> str ! IoError with [Io, Fs] {
    content := read_file(path)?
    content
}
```

### Generics

```aria
fn identity[T](x: T) -> T = x

fn max[T: Ord](a: T, b: T) -> T {
    if a > b { a } else { b }
}

fn process[T: Eq + Hash](val: T) -> bool = true

struct Pair[A, B] {
    first: A
    second: B
}

// Where clause
fn merge[T](a: [T], b: [T]) -> [T] where T: Ord + Clone {
    // ...
}
```

### Structs

```aria
struct Point {
    x: f64
    y: f64
}

// Construction
p := Point{x: 3.0, y: 4.0}

// Field access
println("x = {p.x}")

// Methods via impl
impl Point {
    fn new(x: f64, y: f64) -> Point = Point{x: x, y: y}

    fn distance(self, other: Point) -> f64 {
        dx := self.x - other.x
        dy := self.y - other.y
        // would need sqrt here
        dx * dx + dy * dy
    }
}

// Derives
struct Color {
    r: i64
    g: i64
    b: i64
} derives [Eq, Clone, Debug]
```

### Sum Types (Tagged Unions)

```aria
type Shape =
    | Circle(f64)
    | Rectangle(f64, f64)
    | Triangle(f64, f64, f64)
    | Point

type Result =
    | Ok(i64)
    | Err(str)

type Option =
    | Some(i64)
    | None

// Construction
s := Circle(5.0)
r := Ok(42)
o := Some(10)
```

### Pattern Matching

```aria
// Exhaustive match (compiler enforces all cases covered)
result := match shape {
    Circle(r) => 3.14 * r * r
    Rectangle(w, h) => w * h
    Triangle(a, b, c) => 0.0  // simplified
    Point => 0.0
}

// With guards
match value {
    x if x > 100 => "large"
    x if x > 0 => "positive"
    0 => "zero"
    _ => "negative"
}

// Or-patterns
match color {
    Red | Orange | Yellow => "warm"
    Blue | Green | Purple => "cool"
    _ => "neutral"
}

// Named patterns
match shape {
    s @ Circle(r) => use_shape(s, r)
    _ => default()
}

// Nested patterns
match opt {
    Some(Circle(r)) => r
    Some(_) => 0.0
    None => 0.0
}

// Array patterns
match arr {
    [first, ..rest] => first
    [] => 0
}

// Refutable binding
Ok(value) := try_parse(input) else {
    return default_value
}
```

### Error Handling

```aria
// Functions that can fail use ! for error type
fn parse_int(s: str) -> i64 ! ParseError {
    if s.len() == 0 { return Err(ParseError.Empty) }
    // ... parsing logic ...
    Ok(result)
}

// ? propagates errors (returns Err early)
fn load_config() -> Config ! IoError {
    content := read_file("config.json")?  // returns Err if fails
    parse_json(content)?
}

// ? with error transformation
fn process() -> Data ! AppError {
    raw := fetch(url) ? |e| AppError.Network{source: e}
    parse(raw) ? |e| AppError.Parse{source: e}
}

// catch for inline recovery
config := read_file("config.json") catch |e| {
    "{}"  // fallback value
}

// or for simple defaults
port := parse_int(input) or 8080

// must for required values (panics on error)
db := connect(url) must "database connection required"

// ! for assert-unwrap (panics on Err)
value := parse_int("42")!
```

### Optional Type

```aria
fn find_user(id: i64) -> User? {
    if id == 42 { return Some(user) }
    None
}

// ?? null coalesce
name := find_user(42)??.name ?? "anonymous"

// ?. optional chaining
email := find_user(42)?.email

// Match on optional
match find_user(id) {
    Some(user) => greet(user)
    None => println("not found")
}
```

### Closures

```aria
// Closure types
double := fn(x: i64) -> i64 = x * 2

// As function arguments
fn apply(val: i64, f: fn(i64) -> i64) -> i64 = f(val)
result := apply(5, fn(x: i64) -> i64 = x * x)  // 25

// Capturing variables
offset := 10
add_offset := fn(x: i64) -> i64 = x + offset

// Higher-order functions
fn make_multiplier(n: i64) -> fn(i64) -> i64 {
    fn(x: i64) -> i64 = x * n
}
triple := make_multiplier(3)
triple(7)  // 21
```

### Traits

```aria
trait Display {
    fn display(self) -> str
}

trait Area {
    fn area(self) -> f64
}

// Supertrait
trait Loggable: Display {
    fn log_level(self) -> i64
}

impl Display for Point {
    fn display(self) -> str = "({self.x}, {self.y})"
}

impl Area for Circle {
    fn area(self) -> f64 = 3.14159 * self.radius * self.radius
}

// Trait bounds in functions
fn print_area[T: Area + Display](shape: T) {
    println("{shape.display()}: {shape.area()}")
}

// dyn Trait (dynamic dispatch)
fn process(shape: dyn Area) {
    println("Area: {shape.area()}")
}
```

### Collections

```aria
// Arrays
nums := [1, 2, 3, 4, 5]
nums.len()           // 5
nums[0]              // 1 (0-indexed)
nums.append(6)       // add element

// Maps
m := Map()
m.set("key", "value")
val := m.get("key")
m.has("key")         // true
m.len()              // 1
keys := m.keys()

// Iteration
for item in nums {
    println("{item}")
}
for key in m.keys() {
    println("{key}: {m.get(key)}")
}

// Pipeline operator
result := data
    |> transform
    |> filter_valid
    |> summarize
```

### Control Flow

```aria
// if is an expression
val := if x > 0 { "positive" } else { "non-positive" }

// for loops
for i in 0..10 { println("{i}") }         // 0 to 9
for i in 1..=5 { println("{i}") }         // 1 to 5
for item in collection { process(item) }

// while
mut n := 0
while n < 100 { n = n + 1 }

// loop (infinite, use break to exit)
loop {
    if done() { break }
}

// break and continue
for i in 0..100 {
    if i % 2 == 0 { continue }
    if i > 50 { break }
    println("{i}")
}
```

### Concurrency

```aria
// Spawn a task
task := spawn compute_heavy(data)
result := task.await()

// Structured concurrency — auto-joins at scope exit
scope {
    a := spawn fetch_users()
    b := spawn fetch_orders()
}
// Both a and b are complete here

// Channels
ch := _ariaChanNew(10)    // buffered channel
ch.send(42)
val := ch.recv()
ch.close()

// Iterate over channel
for msg in ch {
    process(msg)
}

// Detached spawn (not auto-joined)
spawn.detach fn() -> i64 {
    background_work()
    0
}

// Select (multiplex channels)
select {
    msg from ch1 => handle(msg)
    msg from ch2 => other(msg)
    default => no_data()
}

// WaitGroup
wg := _ariaWgNew()
_ariaWgAdd(wg, 3)
// ... spawn 3 tasks that call _ariaWgDone(wg) ...
_ariaWgWait(wg)  // blocks until all done
```

### FFI (Foreign Function Interface)

```aria
// Call C functions
extern fn abs(n: i64) -> i64
extern fn strlen(s: i64) -> i64

// Library-specific
extern "sqlite3" {
    fn sqlite3_open(filename: i64, db: i64) -> i64
    fn sqlite3_close(db: i64) -> i64
}

// Usage
result := abs(-42)  // calls C abs()
```

### Memory Management

```aria
// Default: GC-managed (no annotation needed)
data := MyStruct{x: 1, y: 2}

// Stack allocation (no GC, scope-limited)
buf := @stack Buffer.new(4096)

// Arena (bulk allocate, bulk free)
arena := _ariaArenaNew(4096)
// ... allocate many objects ...
_ariaArenaFree(arena)  // free all at once

// Object pool (reuse allocations)
pool := _ariaPoolNew(100, 64)
obj := _ariaPoolGet(pool)
// ... use obj ...
_ariaPoolPut(pool, obj)

// Drop trait for cleanup
impl Drop for Connection {
    fn drop(self) {
        self.close()
    }
}

// with block — deterministic cleanup
with conn := connect(url)? {
    conn.query("SELECT ...")
}  // conn.drop() called here
```

### Annotations

```aria
@deprecated("use new_api instead")
fn old_api() -> i64 = 0

@cold
fn error_handler() { /* rarely called */ }

@stack     // stack-allocate
@arena(a)  // arena-allocate
@inline    // embed in parent struct
@opaque    // FFI opaque pointer type
@cstruct   // C-compatible struct layout
```

### Testing

```aria
test "addition works" {
    assert add(2, 3) == 5
    assert add(-1, 1) == 0
}

test "division by zero" {
    result := safe_div(10, 0)
    match result {
        Ok(_) => assert false
        Err(msg) => assert msg == "division by zero"
    }
}

bench "sorting performance" {
    data := generate_random(10000)
    sort(data)
}
```

### Modules

```aria
mod myapp.server

use std.{fs, json}
use myapp.models.{User, Order}
use myapp.db as database

pub fn start_server(port: i64) {
    // public function
}

fn internal_helper() {
    // private to this module
}
```

---

## Common Patterns

### Builder Pattern
```aria
struct ServerBuilder {
    host: str
    port: i64
    workers: i64
}

impl ServerBuilder {
    fn new() -> ServerBuilder = ServerBuilder{host: "0.0.0.0", port: 8080, workers: 4}
    fn with_host(self, h: str) -> ServerBuilder = ServerBuilder{host: h, port: self.port, workers: self.workers}
    fn with_port(self, p: i64) -> ServerBuilder = ServerBuilder{host: self.host, port: p, workers: self.workers}
    fn build(self) -> Server = Server{host: self.host, port: self.port, workers: self.workers}
}

server := ServerBuilder.new()
    .with_host("localhost")
    .with_port(9090)
    .build()
```

### Error Propagation Chain
```aria
fn process_request(req: Request) -> Response ! AppError {
    user := auth(req) ? |e| AppError.Auth{source: e}
    data := fetch_data(user.id) ? |e| AppError.Data{source: e}
    rendered := render(data) ? |e| AppError.Render{source: e}
    Ok(Response{body: rendered, status: 200})
}
```

### Iterator Pipeline
```aria
fn top_active_users(users: [User], limit: i64) -> [str] {
    mut result := [""]
    mut count := 0
    for user in users {
        if user.active && count < limit {
            result = result.append(user.name)
            count = count + 1
        }
    }
    result
}
```

### Concurrent Fan-Out
```aria
fn fetch_all(urls: [str]) -> [Response] {
    mut tasks := [0]
    for url in urls {
        t := spawn fetch(url)
        tasks = tasks.append(t)
    }
    mut results := [Response{}]
    for t in tasks {
        results = results.append(t.await())
    }
    results
}
```

---

## Translation Guide

### From Rust
| Rust | Aria |
|------|------|
| `let x = 5;` | `x := 5` |
| `let mut x = 5;` | `mut x := 5` |
| `fn foo(x: i32) -> i32 { x + 1 }` | `fn foo(x: i64) -> i64 = x + 1` |
| `Result<T, E>` | `T ! E` |
| `Option<T>` | `T?` |
| `match x { ... }` | `match x { ... }` (same) |
| `enum Foo { A(i32), B }` | `type Foo = \| A(i64) \| B` |
| `impl Trait for Type` | `impl Trait for Type` (same) |
| `x.unwrap()` | `x!` |
| `x?` | `x?` (same) |
| `println!("{}", x)` | `println("{x}")` |
| `vec![1, 2, 3]` | `[1, 2, 3]` |

### From Go
| Go | Aria |
|----|------|
| `x := 5` | `x := 5` (same) |
| `var x int = 5` | `x: i64 = 5` |
| `func foo(x int) int { return x + 1 }` | `fn foo(x: i64) -> i64 = x + 1` |
| `if err != nil { return err }` | `value := expr?` |
| `go func() { ... }()` | `spawn fn() -> i64 { ... }` |
| `ch := make(chan int)` | `ch := _ariaChanNew(1)` |
| `select { case v := <-ch: ... }` | `select { v from ch => ... }` |
| `struct Foo { X int }` | `struct Foo { x: i64 }` |
| `interface Foo { Bar() int }` | `trait Foo { fn bar(self) -> i64 }` |

### From TypeScript/JavaScript
| TypeScript | Aria |
|------------|------|
| `const x = 5` | `x := 5` |
| `let x = 5` | `mut x := 5` |
| `function foo(x: number): number { return x + 1 }` | `fn foo(x: i64) -> i64 = x + 1` |
| `x?.name ?? "default"` | `x?.name ?? "default"` (same!) |
| `try { ... } catch(e) { ... }` | `expr catch \|e\| { ... }` |
| `Promise.all([...])` | `scope { spawn a(); spawn b() }` |
| `{ ...obj, key: value }` | `obj.{key: value}` |
| `type Foo = { x: number }` | `struct Foo { x: i64 }` |
| `type Result = Ok \| Err` | `type Result = \| Ok(i64) \| Err(str)` |

---

## Compiler Commands

```bash
aria check file.aria       # Type-check without compiling
aria build file.aria       # Compile to native executable
aria run file.aria         # Compile and run
aria test file.aria        # Run test blocks
aria bench file.aria       # Run benchmark blocks
aria fix file.aria         # Apply auto-fix suggestions
aria explain E0100         # Explain error code
```
