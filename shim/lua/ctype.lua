function islower(char)
    return char >= 97 and char <= 122
end

function isupper(char)
    return char >= 65 and char <= 90
end

function isalpha(char)
    return islower(char) or isupper(char)
end
