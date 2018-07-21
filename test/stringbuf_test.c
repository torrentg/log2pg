
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string.h"

/*
 * gcc -iquote ../src -o string_test string_test.c ../src/string.c
 */

void test1()
{
  string_t str = {0};
  
  printf("TEST1 --------------------\n");

  string_append(&str, "hola ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  string_append(&str, "don ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_append(&str, "pepito, hola don ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_append(&str, "jose, pepito ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_append(&str, "lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_replace(&str, "pepito", "jose");  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  string_reset(&str);
}

void test2()
{
  string_t str = {0};

  printf("TEST2 --------------------\n");
  
  string_append(&str, "hola don jose, hola don pepito, jose lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_replace(&str, "jose", "pepito");  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  string_reset(&str);
}

void test3()
{
  string_t str = {0};

  printf("TEST3 --------------------\n");
  
  string_append(&str, "hola don xxx, hola don xxx, xxx lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_replace(&str, "xxx", "yyy");  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  string_reset(&str);
}

void test4()
{
  string_t str = {0};

  printf("TEST4 --------------------\n");
  
  string_append(&str, "hola don xxx, hola don xxx, xxx lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  string_replace(&str, "xxx", NULL);  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  string_reset(&str);
}

// main function
int main(int argc, char *argv[])
{
  test1();
  test2();
  test3();
  test4();
  return(0);
}


