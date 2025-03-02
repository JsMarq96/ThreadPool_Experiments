#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * Basic C23 Job System (JS) Thread Pool API using C11 threads or tinyctreads
 * By Juan S. Marquerie (JsMarq96) 16/02/2025
 */

#define MAX_JOB_COUNT_PER_THREAD 2000u

// Forward declaration
struct JS_sThread;
struct JS_sThreadPool;

typedef void (*JS_sJobFunction) (const void*, void*, struct JS_sThreadPool*, const uint8_t);

typedef struct JS_sJobConfig {
    JS_sJobFunction         job_func;
    const void              *read_only_data;
    void                    *read_write_data;
    struct JS_sJobConfig    *parent_job_config;
    bool                    has_parent;
} JS_sJobConfig;

typedef struct JS_sThreadPool {
    struct JS_sThread   *threads;
    uint8_t             thread_count;
} JS_sThreadPool;

void JS_ThreadPool_init(JS_sThreadPool *pool, const uint8_t thread_count);
void JS_ThreadPool_submit_job(JS_sThreadPool *pool, JS_sJobConfig job_data);
void JS_ThreadPool_launch(JS_sThreadPool * pool);
void JS_ThreadPool_wait_for(JS_sThreadPool *pool);
void JS_ThreadPool_clean(JS_sThreadPool *pool);

#endif // __THREAD_POOL_H__
