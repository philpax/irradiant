#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("chained_if 0|1|2\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "0") == 0)
        printf("Argument is zero\n");
    else if (strcmp(argv[1], "1") == 0)
        printf("Argument is one\n");
    else if (strcmp(argv[1], "2") == 0)
        printf("Argument is two\n");
    else
        printf("Argument unknown: %s\n", argv[1]);
}
