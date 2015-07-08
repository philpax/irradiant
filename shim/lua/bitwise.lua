-- Lua 5.2-compatible shim
bin = {}
bin._and = bit32.band
bin._not = bit32.bnot
bin._or  = bit32.bor
bin._xor = bit32.xor
bin._shl = bit32.lshift
bin._shr = bit32.rshift
