# Aria Examples

Example programs demonstrating what Aria can build. All examples compile and run with the self-hosting Aria compiler.

## Running

```bash
# Build and run any example
aria build examples/hello.aria && ./examples/hello

# Or use the self-hosting compiler directly
./src/aria_generated build examples/hello.aria && ./examples/hello
```

## Examples

### hello.aria
**Hello World** — the simplest Aria program.

Entry blocks, string interpolation, `println`.

### fibonacci.aria
**Fibonacci sequence** — recursive function computing fib(0) through fib(15).

Recursion, `if` expressions, `loop` with `break`, string interpolation.

### shapes.aria
**Sum types and pattern matching** — compute areas of circles, rectangles, and triangles.

Sum types with data (`Circle(i64)`), `match` expressions, single-expression functions (`fn area(s) -> i64 = match s { ... }`), array literals.

### calculator.aria
**Recursive descent expression evaluator** — parses and evaluates arithmetic expressions like `2 + 3 * 4` and `((1 + 2) * (3 + 4))` with correct operator precedence.

Sum types for token kinds, structs for parser state, recursive descent parsing, operator precedence climbing, parallel arrays for AST storage, `match` expressions, string character processing (`charAt`, `is_digit`), assertions.

### todo_cli.aria
**Command-line task manager** — add, list, complete, remove, and track todos with file persistence.

```
./examples/todo_cli add "Buy groceries"
./examples/todo_cli list
./examples/todo_cli done 1
./examples/todo_cli stats
```

Structs (`Todo`), sum types (`Priority`), pattern matching, array operations (`append`, iteration), `Map` for key tracking, file I/O (`_ariaReadFile`, `_ariaWriteFile`), string splitting and parsing, command-line arguments (`_ariaArgs`), mutable state.

### json_pipeline.aria
**CSV data pipeline** — reads CSV records, filters by criteria, transforms scores, and generates a report.

Structs for data modeling (`Record`), array-based filter/map/fold operations, higher-order functions, closures (`fn(r: Record) -> bool => r.active`), file I/O, string splitting and parsing, integer formatting (`.toString()`).

### markdown.aria
**Markdown to HTML converter** — converts a Markdown document with headings, bold, italic, inline code, lists, code blocks, and horizontal rules into valid HTML.

String processing (`charAt`, `substring`, `startsWith`, `contains`, `replace`, `indexOf`), character-by-character parsing, escape handling (`escape_html`), state machines for inline formatting, file I/O, assertions to verify output correctness.

### key_value_store.aria
**Persistent key-value dictionary** — set, get, update, and delete key-value pairs with automatic file persistence and reload verification.

`Map` operations (`set`, `get`, `has`, `len`), file I/O for persistence, string serialization/deserialization, integer parsing, round-trip verification with assertions.

### file_processor.aria
**Concurrent file processor** — creates sample files, processes them in parallel with spawned workers, and reports line/word/character counts.

Concurrency (`_ariaSpawn`, `_ariaTaskAwait`), channels (`_ariaChanNew`, `_ariaChanSend`, `_ariaChanRecv`), mutex (`_ariaMutexNew`, `_ariaMutexLock`, `_ariaMutexUnlock`), file system operations (`_ariaReadFile`, `_ariaWriteFile`, `_ariaListDir`, `_ariaIsDir`), recursive directory traversal, string processing, structs for results.

### chat_server.aria
**TCP chat server** — a multi-client chat room that accepts connections, sends welcome messages, and echoes messages back to clients.

```
# In one terminal:
./examples/chat_server

# In another:
nc 127.0.0.1 19090
```

TCP networking (`_ariaTcpSocket`, `_ariaTcpBind`, `_ariaTcpListen`, `_ariaTcpAccept`, `_ariaTcpRead`, `_ariaTcpWrite`), concurrency with `_ariaSpawn` for per-client handlers, channels for event coordination, mutex for shared state, timeouts.

### database.aria
**PostgreSQL CRUD** — connects to a PostgreSQL database, creates a table, inserts/queries/updates/deletes users, and cleans up.

Requires a running PostgreSQL instance with an `aria_example` database (`createdb aria_example`).

FFI via PostgreSQL runtime functions (`_ariaPgConnect`, `_ariaPgExec`, `_ariaPgGetValue`, `_ariaPgNrows`), structs for domain modeling (`User`), error checking, resource cleanup, parameterized queries.

## Language Features Covered

| Feature | Examples |
|---------|----------|
| Entry blocks | all |
| String interpolation | hello, fibonacci, shapes, todo_cli |
| Structs | calculator, todo_cli, json_pipeline, file_processor |
| Sum types | shapes, calculator, todo_cli |
| Pattern matching | shapes, calculator, todo_cli |
| Closures | json_pipeline |
| Arrays | all except hello |
| Map | key_value_store, todo_cli |
| File I/O | todo_cli, json_pipeline, markdown, key_value_store, file_processor |
| Concurrency (spawn/channels) | file_processor, chat_server |
| TCP networking | chat_server |
| FFI (PostgreSQL) | database |
| Recursion | fibonacci, calculator, file_processor |
| String methods | markdown, json_pipeline, key_value_store |
