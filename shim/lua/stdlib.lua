function atoi(str)
    ret = tonumber(str)
    if ret ~= nil then
        return ret
    else
        return 0
    end
end
