#include <stdio.h>

int main(int argc, char** argv)
{
    char const* name = "Irradiant";
    printf("Hello, world from %s", name);
    if (argc > 1)
        printf(" and %s", argv[1]);
    printf("!\n");

    return 0;
}
