#include <stdio.h>
#include <stdlib.h>
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

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

    float x = 3.14159, y = 13.37, z = 42.42;
    printf("Perlin noise at (%f, %f, %f): %f\n", x, y, z, stb_perlin_noise3(x, y, z, 0, 0, 0));

    return 0;
}
