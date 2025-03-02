#include "thread_pool.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "threads_include.h"

// Forward declarations =================================
struct JS_sJob;
struct JS_sJobQueue;
struct JS_sThread;
struct JS_sThreadPool;

typedef struct JS_sJob {
    JS_sJobFunction job_func;
    struct JS_sJob  *parent;
    const void      *read_only_data;
    void            *read_write_data;
} JS_sJob;

typedef struct JS_sJobQueue {
    JS_sJob    queue_ring_buffer[MAX_JOB_COUNT_PER_THREAD];
    int16_t    top_idx;
    int16_t    bottom_idx;
} JS_sJobQueue;

void JS_JobQueue_init(JS_sJobQueue *queue);
bool JS_JobQueue_pop(JS_sJobQueue *queue, JS_sJob **result);
void JS_JobQueue_enqueue(JS_sJobQueue *queue, const JS_sJob new_job);
uint32_t JS_JobQueue_get_size(JS_sJobQueue *queue);

typedef struct JS_sThread {
    uint32_t                thread_id;
    thrd_t                  thread_handle;
    JS_sJobQueue            job_queue;
    struct JS_sThreadPool   *pool;
} JS_sThread;

void JS_Thread_run(JS_sThread *thread);



// IMPLEMENTATION

// Job Queue functions ================================
inline static void JS_JobQueue_init(JS_sJobQueue *queue) {
    queue->top_idx = 0u;
    queue->bottom_idx = 0u;
}

inline static bool JS_JobQueue_pop(JS_sJobQueue *queue, JS_sJob **result) {
    if (queue->bottom_idx == queue->top_idx) {
        return false; // Empty queue
    }

    *result = &queue->queue_ring_buffer[queue->bottom_idx];
    queue->bottom_idx = (queue->bottom_idx + 1u) % MAX_JOB_COUNT_PER_THREAD;

    return true;
}

inline static void JS_JobQueue_enqueue(JS_sJobQueue *queue, const JS_sJob new_job) {
    memcpy(&queue->queue_ring_buffer[queue->top_idx], &new_job, sizeof(JS_sJob));
    queue->top_idx = (queue->top_idx + 1u) % MAX_JOB_COUNT_PER_THREAD;
}

inline static uint32_t JS_JobQueue_get_size(JS_sJobQueue *queue) {
    return abs(queue->bottom_idx - queue->top_idx);
}

// THREAD FUNCTIONS ===================================
// Thread main loop
inline static void JS_Thread_run(JS_sThread *thread) {
    JS_sJobQueue *thread_job_queue = &thread->job_queue;

    // While the Queue is not empty
    JS_sJob *current_job = NULL;
    while(JS_JobQueue_get_size(thread_job_queue) != 0u) {
        if (!JS_JobQueue_pop(thread_job_queue, &current_job)) {
            continue;
        }

        // TODO: send context and other params
        current_job->job_func(  current_job->read_only_data, 
                                current_job->read_write_data, 
                                thread->pool, 
                                thread->thread_id   );

        if (current_job->parent) {
            assert(false && "Parenting not implemented");
            //JS_JobQueue_enqueue(thread_job_queue, current_job->parent);
        }
    }
}

// THREAD POOL ======================================
void JS_ThreadPool_init(JS_sThreadPool *pool, const uint8_t thread_count) {
    pool->threads = (JS_sThread*) malloc(sizeof(JS_sThread) * thread_count);

    pool->thread_count = thread_count;

    for(uint8_t i = 0u; i < thread_count; i++) {
        pool->threads[i].thread_id = i;

        JS_JobQueue_init(&pool->threads[i].job_queue);
    }
}

void JS_ThreadPool_submit_job(JS_sThreadPool *pool, JS_sJobConfig job_data) {
    JS_sThread *threads = pool->threads;
    // Add jobs in a naive round robbin
    for(uint8_t i = 0u; i < pool->thread_count; i++) {
        uint8_t i_next = (i + 1u) % pool->thread_count;
        if (JS_JobQueue_get_size(&threads[i].job_queue) <= JS_JobQueue_get_size(&threads[i_next].job_queue)) {
            JS_JobQueue_enqueue(&threads[i].job_queue, 
                                (JS_sJob) {
                                    .job_func = *job_data.job_func,
                                    .read_only_data = job_data.read_only_data,
                                    .read_write_data = job_data.read_write_data,
                                });

            return;
        }
    }
}

static int start_thread(void * data) {
    JS_Thread_run((JS_sThread*) data);

    return 0u;
}

void JS_ThreadPool_launch(JS_sThreadPool *pool) {
    assert(pool->thread_count > 0u && "Error: launching on empty thread pool");

    for(uint8_t i = 1u; i < pool->thread_count; i++) {
        thrd_create(&pool->threads[i].thread_handle, start_thread, &pool->threads[i]);
        //thrd_detach(&os_threads[i]);
    }

    // Using current thread as the first thread
    JS_Thread_run(&pool->threads[0]);
}

void JS_ThreadPool_wait_for(JS_sThreadPool *pool) {
    // NOTE: only call this from main thread
    int result;
    for(uint8_t i = 1u; i < pool->thread_count; i++) {
        int join_result = thrd_join(pool->threads[i].thread_handle, &result);
        assert(join_result == thrd_success && "Error: join failed");
    }
}

void JS_ThreadPool_clean(JS_sThreadPool *pool) {
    free(pool->threads);
    pool->thread_count = 0u;
}