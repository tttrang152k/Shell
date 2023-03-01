#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(int argc, char * argv[]) {
     char buffer[10];
     fgets(buffer, 10, stdin);
     int n;
     n = atoi(buffer);

     printf("%d \n", n+2); // print n+2

     return 0;

}