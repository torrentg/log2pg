
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

/*
 * gcc -iquote ../src -o utils_test utils_test.c ../src/utils.c
 */

// main function
int main(int argc, char *argv[])
{
  const char *str = "hola don pepito, hola don josé, pepito diodeno";
  
  char *res1 = replace_str(str, "XXXX", "josé");
  printf("%s\n", res1);
  free(res1);
  
  char *res2 = replace_str(str, "pepito", "josé");
  printf("%s\n", res2);
  free(res2);
  
  char *res3 = replace_str(str, "pepito", NULL);
  printf("%s\n", res3);
  free(res3);

  return(0);
}


