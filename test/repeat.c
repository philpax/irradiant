// Sourced from http://rosettacode.org/wiki/Repeat#C
#include <stdio.h>

void repeatFn(void (*f)(void), unsigned int n) {
 while (n-->0)
  (*f)();
}


void repeatFn2(void (*f)(void), unsigned int n) {
 while (n-->0)
  f();
}

void example() {
 printf("Example\n");
}

int main(int argc, char *argv[]) {
 repeatFn(example, 2);
 repeatFn2(example, 2);
 return 0;
}
