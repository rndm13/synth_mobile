#include "ring_buffer.h"

void rb_pop(RingBuffer *rb, RingBufferElem* elem) {
    if (rb->consumed_num == rb->produced_num) {
        // Nothing to consume
        elem->size = 0;
        return;
    }

    *elem = rb->elem_arr[rb->consumed_num];
    rb->consumed_num = (rb->consumed_num + 1) % RB_CAPACITY;
}

void rb_push(RingBuffer *rb, const RingBufferElem* elem) {
    size_t next_produced_num = (rb->produced_num + 1) % RB_CAPACITY;
    if (elem->size <= 0) {
        // Log(LogLevel::Error, "Trying to push size 0 ring buffer element!");
        return;
    }

    rb->elem_arr[rb->produced_num] = *elem;

    if (next_produced_num == rb->consumed_num) {
        // Log(LogLevel::Error, "Ring buffer is overflowing!");
        return;
    }

    rb->produced_num = next_produced_num;
}
