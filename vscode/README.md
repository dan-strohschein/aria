# Aria Language Support for VS Code

Syntax highlighting for the [Aria programming language](https://github.com/aria-lang/aria).

## Features

- Full keyword highlighting (`fn`, `type`, `match`, `entry`, `trait`, `impl`, etc.)
- Primitive type highlighting (`i8`-`i64`, `u8`-`u64`, `f32`, `f64`, `str`, `bool`, etc.)
- String interpolation support (`"hello {name}"`)
- Number literals (decimal, hex, octal, binary, float, duration, size)
- Comment highlighting (`//` line comments, `///` doc comments)
- Operator highlighting (arrows, pipes, optional chaining, ranges)
- PascalCase type name highlighting
- Function call and definition highlighting
- Memory annotation highlighting (`@stack`, `@arena`, `@inline`)
- Bracket matching and auto-closing pairs

## Installation

Search for "Aria Language" in the VS Code Extensions marketplace, or install from the command line:

```
code --install-extension aria-lang.aria-lang
```

## File Association

The extension automatically associates `.aria` files with Aria syntax highlighting.
