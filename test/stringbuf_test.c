
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stringbuf.h"

/*
 * gcc -iquote ../src -o stringbuf_test stringbuf_test.c ../src/stringbuf.c
 */

void test1()
{
  stringbuf_t str = {0};
  
  printf("TEST1 --------------------\n");

  stringbuf_append(&str, "hola ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  stringbuf_append(&str, "don ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_append(&str, "pepito, hola don ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_append(&str, "jose, pepito ");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_append(&str, "lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_replace(&str, "pepito", "jose");  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  stringbuf_reset(&str);
}

void test2()
{
  stringbuf_t str = {0};

  printf("TEST2 --------------------\n");
  
  stringbuf_append(&str, "hola don jose, hola don pepito, jose lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_replace(&str, "jose", "pepito");  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  stringbuf_reset(&str);
}

void test3()
{
  stringbuf_t str = {0};

  printf("TEST3 --------------------\n");
  
  stringbuf_append(&str, "hola don xxx, hola don xxx, xxx lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_replace(&str, "xxx", "yyy");  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  stringbuf_reset(&str);
}

void test4()
{
  stringbuf_t str = {0};

  printf("TEST4 --------------------\n");
  
  stringbuf_append(&str, "hola don xxx, hola don xxx, xxx lomo");
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));

  stringbuf_replace(&str, "xxx", NULL);  
  printf("str=%s, len=%zu, capacity=%zu, strlen=%zu\n", str.data, str.length, str.capacity, strlen(str.data));
  
  stringbuf_reset(&str);
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


