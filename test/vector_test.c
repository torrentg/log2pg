
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vector.h"

#define UNUSED(x) (void)(x)
#define LIST_LENGTH 24

/*
 * gcc -g -I../src -o vector_test vector_test.c ../src/vector.c ../src/utils.c
 * valgrind --tool=memcheck --leak-check=yes ./vector_test
 */

// object to insert
typedef struct witem_t
{
  char *name;
  int info;
} witem_t;

// auxiliary function
witem_t* witem_alloc(const char *name, int info)
{
  witem_t *ret = (witem_t *) malloc(sizeof(witem_t));
  assert(ret != NULL);
  if (ret != NULL) {
    ret->name = strdup(name);
    ret->info = info;
  }
  return(ret);
}

// free object
void witem_free(void *ptr)
{
  if (ptr == NULL) return;
  witem_t *obj = (witem_t *) ptr;
  free(obj->name);
  free(ptr);
}

// auxiliar function
void print_vector(vector_t *lst, const char *msg)
{
  printf("%s = [", msg);
  for(size_t i=0; i<lst->size; i++) {
    witem_t *item = (witem_t *) lst->data[i];
    printf("{%s,%d}, ", item->name, item->info);
  }
  printf("]\n");
}

// main function
int main(int argc, char *argv[])
{
  UNUSED(argc);
  UNUSED(argv);

  int rc = 0;
  int pos = 0;
  char name[100];
  vector_t lst = {0};
  witem_t *item = NULL;

  assert(lst.data == NULL);
  assert(lst.size == 0);
  assert(lst.capacity == 0);

  for(int i=0; i<LIST_LENGTH; i++) {
    sprintf(name, "item%d", i);
    item = witem_alloc(name, i);
    printf("inserting %d-th item: [%s, %d]\n", i+1, item->name, item->info);
    vector_insert(&lst, item);
  }
  print_vector(&lst, "data1");

  assert(lst.size == LIST_LENGTH);
  assert(lst.capacity == 32);

  pos = vector_find(&lst, "not_found");
  assert(pos < 0);

  pos = vector_find(&lst, "item10");
  assert(pos == 10);

  rc = vector_remove(&lst, 10, witem_free);
  print_vector(&lst, "data2");
  assert(rc == 0);
  assert(lst.size == 23);

  pos = vector_find(&lst, "item11");
  assert(pos == 10);

  rc = vector_remove(&lst, 22, witem_free);
  print_vector(&lst, "data3");
  assert(rc == 0);
  assert(lst.size == 22);

  vector_reset(&lst, witem_free);
  assert(lst.data == NULL);
  assert(lst.size == 0);
  assert(lst.capacity == 0);
  return(0);
}
