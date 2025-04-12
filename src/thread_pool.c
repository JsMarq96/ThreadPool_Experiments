#include "thread_pool.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "threads_include.h"

#define THREAD_SUCCESS 1

// Forward declarations =================================
struct JS_sJob;
struct JS_sJobQueue;
struct JS_sThread;
struct JS_sThreadPool;

typedef struct JS_sJob {
    JS_sJobFunction         job_func;
    int32_t                parent_idx;
    const void              *read_only_data;
    void                    *read_write_data;
} JS_sJob;

typedef struct JS_sParentPool {
    atomic_int      parent_counter_buffer[MAX_PARENT_JOB_COUNT_PER_THREAD];
    bool            filled_parents_buffer[MAX_PARENT_JOB_COUNT_PER_THREAD];
    JS_sJobConfig   parents_config[MAX_PARENT_JOB_COUNT_PER_THREAD];
} JS_sParentPool;

typedef struct JS_sJobQueue {
    JS_sJob         queue_ring_buffer[MAX_JOB_COUNT_PER_THREAD];
    int16_t         job_top_idx;
    int16_t         job_bottom_idx;
    uint16_t        queue_size;
} JS_sJobQueue;

void JS_JobQueue_init(JS_sJobQueue *queue);
bool JS_JobQueue_pop(JS_sJobQueue *queue, JS_sJob **result);
void JS_JobQueue_enqueue(JS_sJobQueue *queue, const JS_sJob new_job);

typedef struct JS_sThread {
    thrd_t                  thread_handle;
    JS_sJobQueue            job_queue;
    struct JS_sThreadPool   *pool;
    uint8_t                 thread_id;
    char                    __pad[3];
} JS_sThread;

void JS_Thread_run(JS_sThread *thread, JS_sThreadPool *pool);



// IMPLEMENTATION

// Job Queue functions ================================
inline void JS_JobQueue_init(JS_sJobQueue *queue) {
    queue->job_top_idx = 0u;
    queue->job_bottom_idx = 0u;
    queue->queue_size = 0u;
}

inline bool JS_JobQueue_pop(JS_sJobQueue *queue, JS_sJob **result) {
    if (queue->queue_size == 0u) {
        return false; // Empty queue
    }

    *result = &queue->queue_ring_buffer[queue->job_bottom_idx];
    queue->job_bottom_idx = (queue->job_bottom_idx + 1u) % MAX_JOB_COUNT_PER_THREAD;

    queue->queue_size--;

    return true;
}

inline void JS_JobQueue_enqueue(JS_sJobQueue *queue, const JS_sJob new_job) {
    memcpy(&queue->queue_ring_buffer[queue->job_top_idx], &new_job, sizeof(JS_sJob));
    queue->job_top_idx = (queue->job_top_idx + 1u) % MAX_JOB_COUNT_PER_THREAD;

    queue->queue_size++;
}

// THREAD FUNCTIONS ===================================
// Thread main loop
inline void JS_Thread_run(JS_sThread *thread, JS_sThreadPool* thread_pool) {
    JS_sJobQueue *thread_job_queue = &thread->job_queue;

    // While the Queue is not empty
    JS_sJob *current_job = NULL;
    while(thread_job_queue->queue_size != 0u) {
        if (!JS_JobQueue_pop(thread_job_queue, &current_job)) {
            continue;
        }

        //printf("%u %u\n", thread->thread_id, thread_job_queue->queue_size);

        // TODO: send context and other params
        current_job->job_func(  current_job->read_only_data,
                                current_job->read_write_data,
                                thread->pool,
                                thread->thread_id   );

        int32_t parent_idx = current_job->parent_idx;
        if (parent_idx >= 0) {
            JS_sParentPool *parent_pool = thread_pool->parent_pool;
            int32_t prev_value = atomic_fetch_sub(&parent_pool->parent_counter_buffer[parent_idx], 1);

            if (prev_value == 1) {
                JS_JobQueue_enqueue(thread_job_queue, (JS_sJob) {
                                                        .job_func = *parent_pool->parents_config[parent_idx].job_func,
                                                        .parent_idx = 0u,
                                                        .read_only_data = parent_pool->parents_config[parent_idx].read_only_data,
                                                        .read_write_data = parent_pool->parents_config[parent_idx].read_write_data,
                                                    });
                // JS_ThreadPool_submit_job(thread_pool, parent_pool->parents_config[parent_idx]);
                //                         printf("%d\n", prev_value);

                parent_pool->filled_parents_buffer[parent_idx] = false;
            }
        }
    }
}

// THREAD POOL ======================================
void JS_ThreadPool_init(JS_sThreadPool *pool, const uint8_t thread_count) {
    pool->threads = (JS_sThread*) malloc(sizeof(JS_sThread) * thread_count);
    pool->parent_pool = (JS_sParentPool*) malloc(sizeof(JS_sParentPool));

    // Clean the Parent pool
    memset(pool->parent_pool->filled_parents_buffer, false, sizeof(bool) * MAX_PARENT_JOB_COUNT_PER_THREAD);

    assert(pool->threads && "Error allocating the threads on pool init");

    pool->thread_count = thread_count;

    for(uint8_t i = 0u; i < thread_count; i++) {
        pool->threads[i].thread_id = i;

        JS_JobQueue_init(&pool->threads[i].job_queue);
    }
}

void thread_submit_job(JS_sThreadPool *pool, JS_sJob job_data) {
    JS_sThread *threads = pool->threads;
    // Add jobs in a naive round robbin
    for(uint8_t i = 0u; i < pool->thread_count; i++) {
        uint8_t i_next = (i + 1u) % pool->thread_count;
        if (threads[i].job_queue.queue_size <= threads[i_next].job_queue.queue_size) {
            JS_JobQueue_enqueue(&threads[i].job_queue,
                                job_data);

            return;
        }
    }
}

void JS_ThreadPool_submit_job(JS_sThreadPool *pool, JS_sJobConfig job_data) {
    thread_submit_job(pool, (JS_sJob) {
                                    .job_func = *job_data.job_func,
                                    .parent_idx = -1,
                                    .read_only_data = job_data.read_only_data,
                                    .read_write_data = job_data.read_write_data,
                                });
}

void JS_ThreadPool_submit_jobs_with_parent( JS_sThreadPool *pool,
                                            const uint32_t child_job_count,
                                            JS_sJobConfig *child_jobs_data,
                                            JS_sJobConfig parent_job_data   ) {
    JS_sThread *threads = pool->threads;
    JS_sParentPool *parent_pool = pool->parent_pool;
    JS_sThread *selected_thread = NULL;
    // Search a thread with a naive round robbin
    for(uint8_t i = 0u; i < pool->thread_count; i++) {
        uint8_t i_next = (i + 1u) % pool->thread_count;
        if (threads[i].job_queue.queue_size < threads[i_next].job_queue.queue_size) {
            selected_thread = &threads[i];
            break;
        }
    }

    assert(selected_thread != NULL && "Error: all queues are filled!");

    // TODO: this is really bad. Maybe a empty array position stack
    int32_t available_parent_idx = 0;
    for(; available_parent_idx < MAX_PARENT_JOB_COUNT_PER_THREAD; available_parent_idx++) {
        if (!parent_pool->filled_parents_buffer[available_parent_idx]) {
            break;
        }
    }

    parent_pool->parents_config[available_parent_idx] = parent_job_data;
    parent_pool->filled_parents_buffer[available_parent_idx] = true;

    atomic_init(&parent_pool->parent_counter_buffer[available_parent_idx], child_job_count);

    // Add the jobs to the queue, with the parent
    for(uint32_t j = 0u; j < child_job_count; j++) {
        JS_sJobConfig *child_job = &child_jobs_data[j];

        thread_submit_job(pool, (JS_sJob) {
                                    .job_func = *child_job->job_func,
                                    .parent_idx = available_parent_idx,
                                    .read_only_data = child_job->read_only_data,
                                    .read_write_data = child_job->read_write_data,
                                });
    }
}

static int start_thread(void * data) {
    JS_sThread* thread = (JS_sThread*) data;
    JS_Thread_run(thread, thread->pool);

    return 0u;
}

void JS_ThreadPool_launch(JS_sThreadPool *pool) {
    assert(pool->thread_count > 0u && "Error: launching on empty thread pool");

    for(uint8_t i = 1u; i < pool->thread_count; i++) {
        pool->threads[i].pool = pool;
        thrd_create(&pool->threads[i].thread_handle, start_thread, &pool->threads[i]);
    }

    // Using current thread as the first thread
    JS_Thread_run(&pool->threads[0], pool);
}

void JS_ThreadPool_wait_for(JS_sThreadPool *pool) {
    // NOTE: only call this from main thread
    int result = 0u;
    for(uint8_t i = 1u; i < pool->thread_count; i++) {
        int join_result = thrd_join(pool->threads[i].thread_handle, NULL);
        assert((join_result == THREAD_SUCCESS) && "Error: join failed");
    }
}

void JS_ThreadPool_clean(JS_sThreadPool *pool) {
    free(pool->threads);
    pool->thread_count = 0u;
}
