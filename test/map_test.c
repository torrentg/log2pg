
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "map.h"

#define UNUSED(x) (void)(x)
#define LIST_LENGTH 100

/*
 * gcc -g -I../src  -o map_test map_test.c ../src/map.c
 * valgrind --tool=memcheck --leak-check=yes ./map_test
 */

// auxiliar function
void print_buckets(map_t *map, const char *msg)
{
  printf("%s = ", msg);
  for(size_t i=0; i<map->capacity; i++) {
    if (map->data[i].key < 0) printf("-,");
    else printf("%d,", map->data[i].key);
  }
  printf("\n");
}

void test1()
{
  
  int *items[LIST_LENGTH];
  map_t map = {0};
  int *item = NULL;
  int rc = 0;

  for(int i=0; i<LIST_LENGTH; i++) {
    int *ptr = (int*) malloc(sizeof(int));
    *ptr = i;
    items[i] = ptr;
  }

  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == 0);

  map_reset(&map, free);

  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == 0);

  item = map_find(&map, 0);
  assert(item == NULL);

  rc = map_remove(&map, 0, NULL);
  assert(rc == 1);

  // data1 = -,-,-,-,-,45,-,-
  map_insert(&map, 45, items[45]);
  print_buckets(&map, "data1");
  assert(map.size == 1);
  assert(map.capacity == 8);
  assert(map.data != NULL);

  assert(map_find(&map, 0) == NULL);
  assert(map_find(&map, 5) == NULL);
  assert(map_find(&map, 45) == items[45]);

  // data2 = -,1,2,3,4,45,5,-
  for(size_t i=1; i<=5; i++) {
    map_insert(&map, i, items[i]);
    print_buckets(&map, "data2");
    assert(map.size == i+1);
    assert(map.capacity == 8);
    assert(map_find(&map, 0) == NULL);
    assert(map_find(&map, 45) == items[45]);
    assert(map_find(&map, i) == items[i]);
    assert(map_find(&map, 10) == NULL);
  }

  // data3 = -,1,2,3,4,5,6,-,-,-,-,-,-,45,-,-
  map_insert(&map, 6, items[6]);
  print_buckets(&map, "data3");
  assert(map.size == 7);
  assert(map.capacity == 16);
  assert(map_find(&map, 6) == items[6]);

  map_insert(&map, 6, items[7]);
  print_buckets(&map, "data3");
  assert(map.size == 7);
  assert(map_find(&map, 6) == items[7]);
  map_insert(&map, 6, items[6]);
  assert(map.size == 7);

  // data4 = 64,1,2,3,4,5,6,65,66,67,68,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  for(size_t i=59; i<69; i++) {
    map_insert(&map, i, items[i]);
    print_buckets(&map, "data4");
    assert(map.size == 7+(i-58));
    assert(map_find(&map, i) == items[i]);
  }
  assert(map.size == 17);
  assert(map.capacity == 32);

  // data5 = 64,1,2,3,4,65,6,66,67,68,-,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  rc = map_remove(&map, 5, free);
  print_buckets(&map, "data5");
  assert(rc == 0);
  assert(map.size == 16);
  assert(map.capacity == 32);
  assert(map_find(&map, 5) == NULL);
  rc = map_remove(&map, 5, free);
  assert(rc == 1);

  // data6 = 64,1,2,3,4,65,6,66,67,68,70,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  map_insert(&map, 70, items[70]);
  print_buckets(&map, "data6");
  assert(map.size == 17);
  assert(map_find(&map, 70) == items[70]);
  assert(map_find(&map, 68) == items[68]);

  // data7 = 64,1,2,3,4,65,6,67,68,70,-,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  rc = map_remove(&map, 66, free);
  print_buckets(&map, "data7");
  assert(rc == 0);
  assert(map.size == 16);
  assert(map.capacity == 32);
  assert(map_find(&map, 66) == NULL);
  rc = map_remove(&map, 66, free);
  assert(rc == 1);

  for(int i=1; i<=6; i++) {
    items[i] = NULL;
  }

  items[45] = NULL;

  for(int i=59; i<69; i++) {
    items[i] = NULL;
  }

  items[70] = NULL;

  map_reset(&map, free);
  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == NULL);

  for(int i=0; i<LIST_LENGTH; i++) {
    if (items[i] != NULL) {
      free(items[i]);
    }
  }
}

void test2()
{
  int *items[LIST_LENGTH];
  map_t map = {0};

  for(int i=0; i<LIST_LENGTH; i++) {
    int *ptr = (int*) malloc(sizeof(int));
    *ptr = i;
    items[i] = ptr;
  }

  map_insert(&map, 14, items[14]);
  map_insert(&map, 22, items[22]);
  map_insert(&map, 30, items[30]);
  map_insert(&map, 6, items[6]);
  map_insert(&map, 16, items[16]);
  print_buckets(&map, "data1");
  map_remove(&map, 22, NULL);
  print_buckets(&map, "data2");
  
  map_bucket_t *bucket = NULL;
  map_iterator_t it = {0};

  while((bucket = map_next(&map, &it)) != NULL) {
    int *item = (int *) bucket->value;
    assert(item != NULL);
    map_remove(&map, bucket->key, NULL);
    print_buckets(&map, "data3");
    it.pos = it.pos - (it.pos==0?0:1);
    it.num--;
  }
  
  assert(it.num == 0);

  while((bucket = map_next(&map, &it)) != NULL) {
    assert(0);
  }

  map_reset(&map, NULL);
  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == NULL);

  for(int i=0; i<LIST_LENGTH; i++) {
    free(items[i]);
  }
}

// main function
int main(int argc, char *argv[])
{
  UNUSED(argc);
  UNUSED(argv);

  test1();
  test2();
  
  return(0);
}

