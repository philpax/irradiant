# Irradiant
A Clang-based C to Lua source-to-source compiler.

## Implementation details
Irradiant's currently in the "hacky prototype" stage. It uses a visitor to traverse the Clang AST and directly output Lua code as it goes along - there is no creation of a Lua AST. This approach allows for a quick proof of concept (as C-without-types has a superficial resemblance to Lua), but breaks down when more complicated analysis is required (as is the case for use of the heap, structs, and more.)

This also means that it has to be conservative in the code that it outputs - as it doesn't know the context of each individual expression, it has to produce Lua that works in any context. For example, `a++` is lowered to `(function() local _ = a; a = a + 1; return _; end)()` - even if the result of the increment is otherwise unused. An example of where this approach is excessive:

```c
int i = 0;
while (i < 10)
	i++;
```

will become

```lua
local i = 0
while i < 10 do
    (function() local _ = i; i = i + 1; return _ end)()
end
```

There is considerable room for improvement here. A more advanced optimizer might be able to lift the increment out of the closure, and then remove the closure (as it's dead code).

### Why not LLVM IR?
[Emscripten](https://github.com/kripken/emscripten), the LLVM IR to JS compiler, has proven that using LLVM IR is a viable approach. However, this means the semantics of the original language are lost, and the generated code is not particularly human readable. I wanted to build a C source-to-source compiler in which the original structure of the code was still fundamentally present.

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
* Structs
* The heap
* Basically anything complicated