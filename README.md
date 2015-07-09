# Irradiant
A Clang-based C to Lua source-to-source compiler.

## What works
More or less everything in the test folder (which includes real C code!)

Basic features:
* Functions
* Arrays
* Binary operators (including pre/post increment/decrement operators)
* Some unary operators
* Ternary operators
* If/else statements (including else if)
* While loops
* Do-while loops
* Variable mutation in conditionals
* Switch statements

## What doesn't work
Everything unimplemented, but the big ones:
* Pointers
* Implicit fallthrough in switch statements
