#include "thread_pool.h"

#include <tinycthread.h>
#include <assert.h>

// Job Queue functions ================================
void JS_JobQueue_init(JS_sJobQueue *queue) {
    queue->top_idx = 0u;
    queue->bottom_idx = 0u;
}

bool JS_JobQueue_pop(JS_sJobQueue *queue, JS_sJob **result) {
    if (queue->bottom_idx == queue->top_idx) {
        return false; // Empty queue
    }

    const int16_t new_top = (queue->top_idx - 1u) % MAX_JOB_COUNT_PER_THREAD;
    *result = &queue->queue_ring_buffer[new_top];
    queue->top_idx = new_top;

    return true;
}

void JS_JobQueue_enqueue(JS_sJobQueue *queue, const JS_sJob *new_job) {
    memcpy(queue->queue_ring_buffer + queue->bottom_idx, new_job, sizeof(JS_sJob));
    queue->bottom_idx = (queue->bottom_idx + 1u) % MAX_JOB_COUNT_PER_THREAD;
}

// THREAD FUNCTIONS ===================================
// Thread main loop
void JS_Thread_run(JS_sThread *thread) {
    JS_sJobQueue *thread_job_queue = &thread->job_queue;
    
    // While the Queue is not empty
    JS_sJob *current_job = NULL;
    while(thread_job_queue->bottom_idx != thread_job_queue->top_idx) {
        JS_JobQueue_pop(thread_job_queue, &current_job);

        // TODO: send context and other params
        current_job->job_func(current_job->read_only_data, current_job->read_write_data, thread);

        if (current_job->parent) {
            JS_JobQueue_enqueue(thread_job_queue, current_job->parent);
        }
    }
}

// THREAD POOL ======================================
void JS_ThreadPool_init(JS_sThreadPool *pool, const uint8_t thread_count) {
    pool->threads = (JS_sThread*) malloc(sizeof(JS_sThread) * thread_count);

    pool->os_thread_idx  = (thrd_t*) malloc(sizeof(thrd_t) * thread_count);

    for(uint8_t i = 0u; i < thread_count; i++) {
        pool->threads[i].thread_id = i;

        JS_JobQueue_init(&pool->threads[i].job_queue);
    }
}

int start_thread(void * data) {
    JS_Thread_run((JS_sThread*) data);
}

void JS_ThreadPool_launch(JS_sThreadPool *pool) {
    thrd_t *os_threads = (thrd_t*) pool->os_thread_idx;
    for(uint8_t i = 0u; i < pool->thread_count; i++) {
        thrd_create(&os_threads[i], start_thread, &pool->threads[i]);
    }
}