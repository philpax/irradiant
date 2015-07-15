-- Lua 5.2-compatible shim
bit = {}
bit._and = bit32.band
bit._not = bit32.bnot
bit._or  = bit32.bor
bit._xor = bit32.bxor
bit._shl = bit32.lshift
bit._shr = bit32.rshift