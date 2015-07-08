#include <stdio.h>
#include <stdlib.h>

int add(int a, int b)
{
    return a + b;
}

int main(int argc, char** argv)
{
    char const* name = "Irradiant";
    printf("Hello, world from %s", name);
    if (argc > 1)
        printf(" and %s", argv[1]);
    printf("!\n");

    if (argc > 3)
        printf("Sum: %d\n", add(atoi(argv[2]), atoi(argv[3])));

    return 0;
}
