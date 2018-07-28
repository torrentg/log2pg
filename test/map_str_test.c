
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "map_str.h"

#define UNUSED(x) (void)(x)
#define LIST_LENGTH 100

/*
 * gcc -g -I../src  -o map_str_test map_str_test.c ../src/map_str.c
 * valgrind --tool=memcheck --leak-check=yes ./map_str_test
 */

// auxiliar function
void print_buckets(map_str_t *map, const char *msg)
{
  printf("%s = ", msg);
  for(size_t i=0; i<map->capacity; i++) {
    if (map->data[i].value == NULL) printf("-,");
    else printf("%s,", map->data[i].key);
  }
  printf("\n");
}

void test1()
{
  char buffer[100];
  int *items[LIST_LENGTH];
  map_str_t map = {0};
  int *item = NULL;
  bool rc = true;

  for(int i=0; i<LIST_LENGTH; i++) {
    int *ptr = (int*) malloc(sizeof(int));
    *ptr = i;
    items[i] = ptr;
  }

  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == NULL);

  map_str_reset(&map, free);

  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == NULL);

  item = map_str_find(&map, "key_not_found");
  assert(item == NULL);

  rc = map_str_remove(&map, "key_not_found", NULL);
  assert(rc == false);

  // data1 = -,-,-,-,-,45,-,-
  map_str_insert(&map, "45", items[45]);
  print_buckets(&map, "data1");
  assert(map.size == 1);
  assert(map.capacity == 8);
  assert(map.data != NULL);

  assert(map_str_find(&map, "0") == NULL);
  assert(map_str_find(&map, "5") == NULL);
  assert(map_str_find(&map, "45") == items[45]);

  // data2 = -,1,2,3,4,45,5,-
  for(size_t i=1; i<=5; i++) {
    sprintf(buffer, "%d", i);
    map_str_insert(&map, buffer, items[i]);
    print_buckets(&map, "data2");
    assert(map.size == i+1);
    assert(map.capacity == 8);
    assert(map_str_find(&map, "0") == NULL);
    assert(map_str_find(&map, "45") == items[45]);
    assert(map_str_find(&map, buffer) == items[i]);
    assert(map_str_find(&map, "10") == NULL);
  }

  // data3 = -,1,2,3,4,5,6,-,-,-,-,-,-,45,-,-
  map_str_insert(&map, "6", items[6]);
  print_buckets(&map, "data3");
  assert(map.size == 7);
  assert(map.capacity == 16);
  assert(map_str_find(&map, "6") == items[6]);

  map_str_insert(&map, "6", items[7]);
  print_buckets(&map, "data3");
  assert(map.size == 7);
  assert(map_str_find(&map, "6") == items[7]);
  map_str_insert(&map, "6", items[6]);
  assert(map.size == 7);

  // data4 = 64,1,2,3,4,5,6,65,66,67,68,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  for(size_t i=59; i<69; i++) {
    sprintf(buffer, "%d", i);
    map_str_insert(&map, buffer, items[i]);
    print_buckets(&map, "data4");
    assert(map.size == 7+(i-58));
    assert(map_str_find(&map, buffer) == items[i]);
  }
  assert(map.size == 17);
  assert(map.capacity == 32);

  // data5 = 64,1,2,3,4,65,6,66,67,68,-,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  rc = map_str_remove(&map, "5", free);
  print_buckets(&map, "data5");
  assert(rc == true);
  assert(map.size == 16);
  assert(map.capacity == 32);
  assert(map_str_find(&map, "5") == NULL);
  rc = map_str_remove(&map, "5", free);
  assert(rc == false);

  // data6 = 64,1,2,3,4,65,6,66,67,68,70,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  map_str_insert(&map, "70", items[70]);
  print_buckets(&map, "data6");
  assert(map.size == 17);
  assert(map_str_find(&map, "70") == items[70]);
  assert(map_str_find(&map, "68") == items[68]);

  // data7 = 64,1,2,3,4,65,6,67,68,70,-,-,-,45,-,-,-,-,-,-,-,-,-,-,-,-,-,59,60,61,62,63,
  rc = map_str_remove(&map, "66", free);
  print_buckets(&map, "data7");
  assert(rc == true);
  assert(map.size == 16);
  assert(map.capacity == 32);
  assert(map_str_find(&map, "66") == NULL);
  rc = map_str_remove(&map, "66", free);
  assert(rc == false);

  for(int i=1; i<=6; i++) {
    items[i] = NULL;
  }

  items[45] = NULL;

  for(int i=59; i<69; i++) {
    items[i] = NULL;
  }

  items[70] = NULL;

  map_str_reset(&map, free);
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
  map_str_t map = {0};

  for(int i=0; i<LIST_LENGTH; i++) {
    int *ptr = (int*) malloc(sizeof(int));
    *ptr = i;
    items[i] = ptr;
  }

  map_str_insert(&map, "14", items[14]);
  map_str_insert(&map, "22", items[22]);
  map_str_insert(&map, "30", items[30]);
  map_str_insert(&map, "6", items[6]);
  map_str_insert(&map, "16", items[16]);
  print_buckets(&map, "data1");
  map_str_remove(&map, "22", NULL);
  print_buckets(&map, "data2");
  
  map_str_bucket_t *bucket = NULL;
  map_str_iterator_t it = {0};

  while((bucket = map_str_next(&map, &it)) != NULL) {
    char *item = (char *) bucket->value;
    assert(item != NULL);
    map_str_remove(&map, bucket->key, NULL);
    print_buckets(&map, "data3");
    it.pos = it.pos - (it.pos==0?0:1);
    it.num--;
  }
  
  assert(it.num == 0);

  while((bucket = map_str_next(&map, &it)) != NULL) {
    assert(0);
  }

  map_str_reset(&map, NULL);
  assert(map.size == 0);
  assert(map.capacity == 0);
  assert(map.data == NULL);

  for(int i=0; i<LIST_LENGTH; i++) {
    free(items[i]);
  }
}

uint32_t map_str_hash(const char *str)
{
  uint32_t ret = 5381U;
  const unsigned char *p = (const unsigned char *) str;

  while (*p != '\0') {
    ret = (ret << 5) + ret + *p;
    ++p;
  }

  return ret;
}

void test3()
{
  char buffer[100];
  
  for(int i=0; i<=10; i++) {
    sprintf(buffer, "%d", i);
    printf("hash[%d] = %zu\n", i, map_str_hash(buffer));
  }
  
  printf("hash[a] = %zu\n", map_str_hash("a"));
  printf("hash[b] = %zu\n", map_str_hash("b"));
  printf("hash[c] = %zu\n", map_str_hash("c"));
  printf("hash[ab] = %zu\n", map_str_hash("ab"));
  printf("hash[abc] = %zu\n", map_str_hash("abc"));
}

// main function
int main(int argc, char *argv[])
{
  UNUSED(argc);
  UNUSED(argv);

  test1();
  test2();
  test3();
  
  return(0);
}

