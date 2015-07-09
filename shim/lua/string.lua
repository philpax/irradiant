function strcmp(str1, str2)
    if str1 < str2 then
        return -1
    elseif str1 > str2 then
        return 1
    else
        return 0
    end
end
