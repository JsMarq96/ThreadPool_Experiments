#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * Basic C23 Job System (JS) Thread Pool API using C11 threads or tinyctreads
 * By Juan S. Marquerie (JsMarq96) 16/02/2025
 */

#define MAX_JOB_COUNT_PER_THREAD 2000u

// Forward declarations
struct JS_sJob;
struct JS_sJobQueue;
struct JS_sThread;
struct JS_sThreadPool;
struct JS_sJobConfig;

typedef void (*JS_sJobFunction) (const void*, void*, struct JS_sThread*);

typedef struct JS_sJobConfig {
    bool                    has_parent;
    JS_sJobFunction         job_func;
    const void              *read_only_data;
    void                    *read_write_data;
    struct JS_sJobConfig    *parent_job_config;
} JS_sJobConfig;

typedef struct JS_sJob {
    void            (*job_func) (const void*, void*, struct JS_sThread*);
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
    JS_sJobQueue            job_queue;
    struct JS_sThreadPool   *pool;
} JS_sThread;

void JS_Thread_run(JS_sThread *thread);

typedef struct JS_sThreadPool {
    uint8_t     thread_count;
    JS_sThread  *threads;
    void        *os_thread_idx;
} JS_sThreadPool;

void JS_ThreadPool_init(JS_sThreadPool *pool, const uint8_t thread_count);
void JS_ThreadPool_submit_job(JS_sThreadPool *pool, JS_sJobConfig job_data);
void JS_ThreadPool_submit_jobs(JS_sThreadPool *pool, JS_sJob *jobs, const uint16_t job_count);
void JS_ThreadPool_launch(JS_sThreadPool * pool);
void JS_ThreadPool_wait_for(JS_sThreadPool *pool);
void JS_ThreadPool_clean(JS_sThreadPool *pool);

#endif // __THREAD_POOL_H__