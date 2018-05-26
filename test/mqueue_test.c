
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "mqueue.h"

#define UNUSED(x) (void)(x)
#define DATA_LENGTH 100

/*
 * gcc -g -o mqueue_test mqueue_test.c mqueue.c -lpthread
 * valgrind --tool=memcheck --leak-check=yes ./mqueue_test
 */

int value = 9999;

size_t mqueue_size(const mqueue_t *mqueue)
{
  if (mqueue->status == 1) {
    return(0);
  }
  
  int ret = (int)(mqueue->pos2) - (int)(mqueue->pos1) + 1;
  if (ret <= 0) {
    ret += mqueue->capacity;
  }
  
  return(ret);
}

int get_sem1_value(mqueue_t *mqueue)
{
  int val = 0;
  sem_getvalue(&(mqueue->sem1), &val);
  return(val);
}

int get_sem2_value(mqueue_t *mqueue)
{
  int val = 0;
  sem_getvalue(&(mqueue->sem2), &val);
  return(val);
}

void print_mqueue(mqueue_t *mqueue, const char *msg)
{
  printf("%s = [buffer=", msg);
  for(size_t i=0; i<mqueue->capacity; i++) {
    if (mqueue->buffer[i] == NULL) printf("-,");
    else printf("%d,", *((int *)mqueue->buffer[i]));
  }
  printf("; pos1=%zu; pos2=%zu; status=%d; size=%zu]\n", mqueue->pos1, mqueue->pos2, (int)mqueue->status, mqueue_size(mqueue));
}

// test mqueue without blocks
void test1()
{
  int rc = 0;
  mqueue_t mqueue = {0};
  int items[DATA_LENGTH];
  void *item = NULL;

  for(int i=0; i<DATA_LENGTH; i++) {
    items[i] = i;
  }
  
  rc = mqueue_init(&mqueue, 0);
  assert(rc == 0);
  assert(mqueue.capacity == 8);

  //item = mqueue_pop(&mqueue, 0);
  //assert(item == NULL);

  print_mqueue(&mqueue, "data1");
  
  for(int i=0; i<10; i++) {
    mqueue_push(&mqueue, &(items[i]), false, 0);
    assert(mqueue.buffer[i] == &(items[i]));
  }
  
  print_mqueue(&mqueue, "data2");
  assert(mqueue.capacity == 16);
  assert(mqueue_size(&mqueue)==10);
  
  item = mqueue_pop(&mqueue, 0);
  assert(item == &(items[0]));
  assert(mqueue_size(&mqueue)==9);

  item = mqueue_pop(&mqueue, 0);
  assert(item == &(items[1]));
  assert(mqueue_size(&mqueue)==8);
  
  print_mqueue(&mqueue, "data3");

  for(int i=10; i<24; i++) {
    mqueue_push(&mqueue, &(items[i]), false, 0);
    print_mqueue(&mqueue, "data3-x");
  }

  print_mqueue(&mqueue, "data4");
  assert(mqueue.pos1 == 0);
  assert(mqueue_size(&mqueue)==22);

  for(int i=0; i<10; i++) {
    item = mqueue_pop(&mqueue, 0);
    assert(item == &(items[i+2]));
  }

  print_mqueue(&mqueue, "data5");

  for(int i=24; i<32+5; i++) {
    mqueue_push(&mqueue, &(items[i]), false, 0);
    assert(mqueue.buffer[(i-2)%32] == &(items[i]));
  }

  print_mqueue(&mqueue, "data6");

  for(int i=32+5; i<48; i++) {
    mqueue_push(&mqueue, &(items[i]), false, 0);
  }

  print_mqueue(&mqueue, "data7");

  assert(mqueue.capacity == 64);

  for(int i=0; i<36; i++) {
    assert(mqueue.buffer[i] == &(items[i+12]));
  }
  for(int i=38; i<64; i++) {
    assert(mqueue.buffer[i] == NULL);
  }

  mqueue_reset(&mqueue);
  assert(mqueue.buffer == NULL);
  assert(mqueue.capacity == 0);
}

void* test1_producer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    mqueue_push(mqueue, &value, false, 0);
    int sem1 = get_sem1_value(mqueue);
    int sem2 = get_sem2_value(mqueue);
    size_t size = mqueue_size(mqueue);
    printf("producer push(%zu), size=%zu, sem1=%d, sem2=%d\n", i+1, size, sem1, sem2);
  }
  mqueue_close(mqueue);
  return(NULL);
}

void* test1_consumer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    sleep(1);
    void *ptr = mqueue_pop(mqueue, 0);
    int sem1 = get_sem1_value(mqueue);
    int sem2 = get_sem2_value(mqueue);
    size_t size = mqueue_size(mqueue);
    printf("consumer pop(%zu), size=%zu, sem1=%d, sem2=%d\n", i+1, size, sem1, sem2);
    if (ptr == MQUEUE_ITEM_CLOSE) break;
  }
  return(NULL);
}

// +producer, -consumer, wait=0
void test2()
{
  mqueue_t mqueue = {0};
  pthread_t producer;
  pthread_t consumer;

  mqueue_init(&mqueue, 10);

  printf("TEST2------------\n");
  pthread_create(&producer, NULL, test1_producer, (void*)(&mqueue));
  pthread_create(&consumer, NULL, test1_consumer, (void*)(&mqueue));
  pthread_join(producer, NULL);
  pthread_join(consumer, NULL);
  
  mqueue_reset(&mqueue);
}

void* test2_producer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    sleep(1);
    mqueue_push(mqueue, &value, false, 0);
    int sem1 = get_sem1_value(mqueue);
    int sem2 = get_sem2_value(mqueue);
    size_t size = mqueue_size(mqueue);
    printf("producer push(%zu), size=%zu, sem1=%d, sem2=%d\n", i+1, size, sem1, sem2);
  }
  return(NULL);
}

void* test2_consumer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    void *ptr = mqueue_pop(mqueue, 0);
    int sem1 = get_sem1_value(mqueue);
    int sem2 = get_sem2_value(mqueue);
    size_t size = mqueue_size(mqueue);
    printf("consumer pop(%zu), size=%zu, sem1=%d, sem2=%d\n", i+1, size, sem1, sem2);
    if (ptr == MQUEUE_ITEM_CLOSE) break;
  }
  return(NULL);
}

// -producer, +consumer, wait=0
void test3()
{
  mqueue_t mqueue = {0};
  pthread_t producer;
  pthread_t consumer;

  mqueue_init(&mqueue, 10);

  printf("TEST3------------\n");
  pthread_create(&producer, NULL, test2_producer, (void*)(&mqueue));
  pthread_create(&consumer, NULL, test2_consumer, (void*)(&mqueue));
  pthread_join(producer, NULL);
  pthread_join(consumer, NULL);

  mqueue_reset(&mqueue);
}

int main(int argc, char *argv[])
{
  UNUSED(argc);
  UNUSED(argv);

  test1();
  test2();
  test3();
  return(0);
}
