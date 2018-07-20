
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "utils.h"
#include "mqueue.h"

#define UNUSED(x) (void)(x)
#define DATA_LENGTH 100

/*
 * gcc -g -I../src -o mqueue_test mqueue_test.c ../src/mqueue.c ../src/utils.c -lpthread
 * valgrind --tool=memcheck --leak-check=yes ./mqueue_test
 */

int value = 9999;

size_t mqueue_size(const mqueue_t *mqueue)
{
  if (mqueue->status == MQUEUE_STATUS_EMPTY) {
    return(0);
  }
  
  int ret = (int)(mqueue->pos2) - (int)(mqueue->pos1) + 1;
  if (ret <= 0) {
    ret += mqueue->capacity;
  }
  
  return(ret);
}

void print_mqueue(mqueue_t *mqueue, const char *msg)
{
  printf("%s = [buffer=", msg);
  for(size_t i=0; i<mqueue->capacity; i++) {
    if (mqueue->buffer[i].data == NULL) printf("-,");
    else printf("%d,", *((int *)mqueue->buffer[i].data));
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
  msg_t msg = {0};

  for(int i=0; i<DATA_LENGTH; i++) {
    items[i] = i;
  }
  
  rc = mqueue_init(&mqueue, "mqueue1", 0);
  assert(rc == 0);
  assert(mqueue.capacity == 8);

  print_mqueue(&mqueue, "data1");
  
  for(int i=0; i<10; i++) {
    mqueue_push(&mqueue, MSG_TYPE_FILE0, &(items[i]), false, 0);
    assert(mqueue.buffer[i].data == &(items[i]));
  }
  
  print_mqueue(&mqueue, "data2");
  assert(mqueue.capacity == 16);
  assert(mqueue_size(&mqueue)==10);
  
  msg = mqueue_pop(&mqueue, 0);
  assert(msg.data == &(items[0]));
  assert(mqueue_size(&mqueue)==9);

  msg = mqueue_pop(&mqueue, 0);
  assert(msg.data == &(items[1]));
  assert(mqueue_size(&mqueue)==8);
  
  print_mqueue(&mqueue, "data3");

  for(int i=10; i<24; i++) {
    mqueue_push(&mqueue, MSG_TYPE_FILE0, &(items[i]), false, 0);
    print_mqueue(&mqueue, "data3-x");
  }

  print_mqueue(&mqueue, "data4");
  assert(mqueue.pos1 == 0);
  assert(mqueue_size(&mqueue)==22);

  for(int i=0; i<10; i++) {
    msg = mqueue_pop(&mqueue, 0);
    assert(msg.data == &(items[i+2]));
  }

  print_mqueue(&mqueue, "data5");

  for(int i=24; i<32+5; i++) {
    mqueue_push(&mqueue, MSG_TYPE_FILE0, &(items[i]), false, 0);
    assert(mqueue.buffer[(i-2)%32].data == &(items[i]));
  }

  print_mqueue(&mqueue, "data6");

  for(int i=32+5; i<48; i++) {
    mqueue_push(&mqueue, MSG_TYPE_FILE0, &(items[i]), false, 0);
  }

  print_mqueue(&mqueue, "data7");

  assert(mqueue.capacity == 64);

  for(int i=0; i<36; i++) {
    assert(mqueue.buffer[i].data == &(items[i+12]));
  }
  for(int i=38; i<64; i++) {
    assert(mqueue.buffer[i].data == NULL);
  }

  mqueue_reset(&mqueue, NULL);
  assert(mqueue.buffer == NULL);
  assert(mqueue.capacity == 0);
}

void* test2_producer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    mqueue_push(mqueue, MSG_TYPE_FILE0, &value, false, 0);
    size_t size = mqueue_size(mqueue);
    printf("producer push(%zu), size=%zu\n", i+1, size);
  }
  mqueue_close(mqueue);
  return(NULL);
}

void* test2_consumer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    sleep(1);
    msg_t msg = mqueue_pop(mqueue, 0);
    size_t size = mqueue_size(mqueue);
    printf("consumer pop(%zu), size=%zu\n", i+1, size);
    if (msg.type == MSG_TYPE_CLOSE) break;
  }
  return(NULL);
}

// +producer, -consumer, wait=0
void test2()
{
  mqueue_t mqueue = {0};
  pthread_t producer;
  pthread_t consumer;

  mqueue_init(&mqueue, "mqueue2", 10);

  printf("TEST2------------\n");
  pthread_create(&producer, NULL, test2_producer, (void*)(&mqueue));
  pthread_create(&consumer, NULL, test2_consumer, (void*)(&mqueue));
  pthread_join(producer, NULL);
  pthread_join(consumer, NULL);
  
  mqueue_reset(&mqueue, NULL);
}

void* test3_producer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    sleep(1);
    mqueue_push(mqueue, MSG_TYPE_FILE0, &value, false, 0);
    size_t size = mqueue_size(mqueue);
    printf("producer push(%zu), size=%zu\n", i+1, size);
  }
  return(NULL);
}

void* test3_consumer(void *ptr)
{
  mqueue_t *mqueue = (mqueue_t *) ptr;
  for(size_t i=0; i<18; i++) {
    msg_t msg = mqueue_pop(mqueue, 0);
    size_t size = mqueue_size(mqueue);
    printf("consumer pop(%zu), size=%zu\n", i+1, size);
    if (msg.type == MSG_TYPE_CLOSE) break;
  }
  return(NULL);
}

// -producer, +consumer, wait=0
void test3()
{
  mqueue_t mqueue = {0};
  pthread_t producer;
  pthread_t consumer;

  mqueue_init(&mqueue, "mqueue3", 10);

  printf("TEST3------------\n");
  pthread_create(&producer, NULL, test3_producer, (void*)(&mqueue));
  pthread_create(&consumer, NULL, test3_consumer, (void*)(&mqueue));
  pthread_join(producer, NULL);
  pthread_join(consumer, NULL);

  mqueue_reset(&mqueue, NULL);
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
