# llvmpy v3.0
[中文 (Chinese)](./README.md) | English

# Introduction
**This is a front-end compiler for `Python`, primarily used to compile `Python` code into `LLVM` `IR` code. The final process of generating an executable file is completed by `Clang`.**

Currently, this is merely a toy project. To implement a real compiler, one shouldn't start coding right away. Instead, one should first understand various `Python` syntaxes and features, then determine the overall architecture before starting to write code. However, I took the opposite approach, implementing a simple `Python` compiler based on my own impressions. It probably doesn't even qualify as a subset of `Python`, as many features differ from the original `Python`. I don't know how long my interest will sustain this project.

---
## Features
> Currently, this project is implemented entirely based on the features required by `example.py`. Other features have not been implemented yet. Therefore, there isn't much to introduce at the moment.

### Data Types
- [x] High-precision arithmetic (GMP default 256-bit precision):
  - [x] Supports `float` floating-point numbers
  - [x] Supports `int` integers
  - [x] Supports `bool` boolean values
  - [ ] Supports `complex` complex numbers (No plans yet)

### Special Types
- [x] Supports `None` null value
- [x] Supports `Any` type
- [x] Supports function objects
- [ ] Custom class objects (Partially supported, see Syntax -> class)
- [ ] Supports `Ellipsis` (No plans yet)

### Containers
- [x] Supports `string`
  - [x] Supports escape characters
  - [x] Supports string concatenation
  - [ ] `f-string` (No plans yet)
- [x] Supports `list`
  - [x] Supports multi-dimensional lists, supports mixed types
- [x] Supports `dict`
  - [x] Supports mixed-type **key-value** pairs (Anything hashable in RunTime can be a key-value)
- [ ] Supports `set` (No plans yet)
- [ ] Supports `tuple` (No plans yet)
---

### Syntax
- [x] Supports complex **arithmetic operators (add, subtract, multiply, divide)**
  - [x] Supports `string` concatenation
  - [x] Supports `list` concatenation and repetition (e.g., `[1,2] + [3,4]*2`)
- [x] Supports most comparison operators:
  - [x] Supports `==` `!=` `>` `<` `>=` `<=`
  - [ ] Supports logical short-circuiting (for `and`, `or`)
  - [ ] Supports `is` `in` `not in` `is not`
  - [ ] Supports bitwise operators `&` `|` `^` `<<` `>>` (Supported by Lexer and RunTime, but not by Parser and CodeGen)
  - [ ] Supports comparison of container objects
- [x] Supports `if-elif-else` statements
- [x] Supports `def` function definitions:
  - [x] Supports generics (type hints)
  - [x] Supports function objects
  - [x] Supports nested function definitions
- [ ] Supports `lambda` expressions
- [ ] Supports list comprehensions

- [x] Supports `while-else` statements
- [x] Supports `for-else` statements
- [ ] Supports `try-except` statements
- [ ] Supports `with` statements

- [~] Supports `class` (Basic class definition parsing is supported. Full features like inheritance, methods are work in progress)

- [ ] Supports `async` `await` statements (No plans yet)

## Multiple Files
- [x] Currently only supports single-file execution with `main()` as an entry point, or global execution.
- [ ] Supports multi-file compilation
- [ ] Supports `import`, `from-import`, `import as`, `from-import as` statements
- [ ] Supports `__name__`, `__main__` (No plans yet)
- [ ] Supports `__all__` (No plans yet)
- [ ] Supports `__init__.py` (No plans yet)
---
## Test Cases
For detailed feature tests, please refer to the test cases in `tests_auto/`.
`alltest.sh` is an automated testing script that can run all test cases:
- `tests_auto/checkneeds`: Checks if basic functionalities like `return 0`, `return 1`, `if-else` are working correctly, as they are dependencies for other tests. These are special, so test cases here require a corresponding `*.sh` script for special handling.
- `tests_auto/`: Tests complex features, implemented using its own `if-else-if` statements, thus requiring support from `tests_auto/checkneeds`.

## Simple Usage
`compile.sh` is a simple compilation script that can directly compile the `./test.py` file and run it.

## About Performance
- Uses `GMP`'s `mpz` for high-precision arithmetic, but performance is somewhat poor because `mpz` is much slower than native `int`.
- The `LLVM` `IR` code generator is a simple implementation, so performance is somewhat poor due to lack of many optimizations.
- Uses `libffi` for calls with unknown signatures, which is about 1.5 times slower than native `C` function calls.
- Static analysis is very poorly done.

## License
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

This project is licensed under the
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).