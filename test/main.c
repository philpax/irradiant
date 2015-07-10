#include <stdio.h>
#include <stdlib.h>
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

int add(int a, int b)
{
    return a + b;
}

int static_increment()
{
    static int x = 0, y = 0;
    return ++x + y;
}

int main(int argc, char** argv)
{
    enum IrradiantStatus
    {
        IRRADIANTSTATUS_BAD,
        IRRADIANTSTATUS_OK = 2,
        IRRADIANTSTATUS_GOOD
    };

    char const* name = "Irradiant";
    printf("Hello, world from %s", name);
    if (argc > 1)
        printf(" and %s", argv[1]);
    printf("!\n");

    for (int i = 0; i < 3; ++i)
        printf("static_increment: %d\n", static_increment());

    if (argc > 3)
        printf("Sum: %d\n", add(atoi(argv[2]), atoi(argv[3])));

    float x = 3.14159, y = 13.37, z = 42.42;
    printf("Perlin noise at (%f, %f, %f): %f\n", x, y, z, stb_perlin_noise3(x, y, z, 0, 0, 0));

    printf("Irradiant status: %d\n", IRRADIANTSTATUS_GOOD);

    return 0;
}
