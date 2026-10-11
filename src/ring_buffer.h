#pragma once

#include <stddef.h>
#include <stdint.h>

#define RB_CAPACITY 1024
#define RB_ELEM_MAX_SIZE 512

typedef struct RingBufferElem {
    size_t size;
    uint8_t data[RB_ELEM_MAX_SIZE];
} RingBufferElem;

// This is only suited for 1 consumer 1 producer but it should be fine.
typedef struct RingBuffer {
    size_t produced_num;
    size_t consumed_num;
    RingBufferElem elem_arr[RB_CAPACITY];
} RingBuffer;

void rb_pop(RingBuffer *rb, RingBufferElem* elem);
void rb_push(RingBuffer *rb, const RingBufferElem* elem);
