typedef struct {} FILE;

int printf(char const*, ...);
int getchar();
int putchar(int);
int fprintf(FILE*, char const*, ...);

FILE* stdout;
FILE* stderr;
int EOF = -1;
