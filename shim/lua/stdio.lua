function getchar()
    local ret = io.read(1)
    if ret == nil then
        return -1
    else
        return string.byte(ret)
    end
end

function putchar(char)
    io.write(string.char(char))
end

function fprintf(file, str, ...)
    str = str:gsub("%%[Ll]", "%%")
    file:write(str:format(...))
end

function printf(str, ...)
   return fprintf(stdout, str, ...)
end

stdout = io.stdout
stderr = io.stderr
EOF = -1
