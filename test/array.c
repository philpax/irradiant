#include <stdio.h>

int main()
{
	int arr1[32];
	int arr2[32] = {1};
	int arr3[3] = {1, 2, 3};

	printf("arr1[16] = %d\n", arr1[16]);
	printf("arr2[16] = %d\n", arr2[16]);
	printf("arr3 = {%d, %d, %d}\n", arr3[0], arr3[1], arr3[2]);
}