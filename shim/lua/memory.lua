-- Replace with more efficient method if available
mem = {}
function mem.make_array(size, initializer)
	local ret = {}
	for i = 1, size do
		ret[i] = 0
	end

	if initializer ~= nil and type(initializer) == "table" then
		for i, v in ipairs(initializer) do
			ret[i] = v
		end
	end

	return ret
end