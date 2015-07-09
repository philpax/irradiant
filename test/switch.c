#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("switch 0|1|2|3|4\n");
        return EXIT_FAILURE;
    }

    int num = atoi(argv[1]);

    switch (num)
    {
    case 0:
    case 4:
    case 1:
        printf("Argument is zero, one or four\n");
        break;
    case 2:
        printf("Argument is 2\n");
    case 3:
        printf("Implicit fallthrough is fun!\n");
        break;
    default:
        printf("Argument is unknown: %d\n", num);
        break;
    };

    printf("To the end\n");
}
