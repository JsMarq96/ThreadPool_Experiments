#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "threads_include.h"
#include "thread_pool.h"

// TODO: get this programatically
#define THREAD_COUNT 10u
#define TO_DO_PER_JOB 2000u
#define ARRAY_COUNT (THREAD_COUNT * TO_DO_PER_JOB * 100u)
#define JOB_COUNT (ARRAY_COUNT / TO_DO_PER_JOB)

#define GET_DOUBLE_MS_FROM_TIME(time_struct) ((double) time_struct.tv_sec * 1e9 + time_struct.tv_nsec)
#define TIME_BLOCK(label, block_to_eval) {\
    struct timespec begin_time, end_time;\
    clock_gettime(CLOCK_REALTIME, &begin_time);\
    block_to_eval;\
    clock_gettime(CLOCK_REALTIME, &end_time);\
    double f = GET_DOUBLE_MS_FROM_TIME(end_time) - GET_DOUBLE_MS_FROM_TIME(begin_time); \
    printf("%s: %f ms\n",label, f/1000000); \
}

typedef struct sJobParams {
    uint32_t write_to_idx;
    uint32_t startin_idx;
    uint32_t *values;
} sJobParams;

static void Job_sum_func(const void* read_only, void* read_write, struct JS_sThreadPool* pool, const uint8_t thread_id) {
    const sJobParams *params = read_only;
    const uint32_t *values = params->values;
    const uint32_t starting_idx = params->startin_idx;

    uint32_t count = 0u;
    for(uint32_t i = 0u; i < TO_DO_PER_JOB; i++) {
        count += values[starting_idx + i];
    }

    ((uint32_t*)read_write)[params->write_to_idx] += count;
    //thrd_sleep(&(struct timespec){.tv_sec=0.0001}, NULL);
}

int main(void) {
    uint32_t base_values[ARRAY_COUNT];
    uint32_t results[JOB_COUNT];
    sJobParams params[JOB_COUNT];

    // Prepare the problem first problem
    for(uint32_t i = 0u; i < ARRAY_COUNT; i++) {
        base_values[i] = 1u;
        if (i < JOB_COUNT) {
            results[i] = 0u;
        }
    }

    JS_sThreadPool job_pool;
    JS_ThreadPool_init(&job_pool, THREAD_COUNT);

    for(uint32_t i = 0u; i < JOB_COUNT; i++) {
        params[i] = (sJobParams){
            .write_to_idx = i,
            .startin_idx = i * TO_DO_PER_JOB,
            .values = base_values,
        };

        JS_ThreadPool_submit_job(&job_pool, 
                                (JS_sJobConfig) {
                                    .job_func = &Job_sum_func,
                                    .read_only_data = (void*) &params[i],
                                    .read_write_data = (void*) results,
                                });
    }

    TIME_BLOCK( "Test 1 multithreaded",
                JS_ThreadPool_launch(&job_pool);
                JS_ThreadPool_wait_for(&job_pool));

    uint32_t result = 0u;

    for(uint32_t i = 0u; i < JOB_COUNT; i++) {
        result += results[i];
    }

    TIME_BLOCK( "Test 1 secuential",
         for(uint32_t i = 0u; i < JOB_COUNT; i++) {   
            Job_sum_func(&params[i], results, NULL, 0);
        }
    );

    printf("Test 1: Result: %d / %d from %d jobs in %d threads\n", result, ARRAY_COUNT, JOB_COUNT, THREAD_COUNT);

    // Prepare the second problem
    for(uint32_t i = 0u; i < JOB_COUNT; i++) {
        results[i] = 0u;
    }

    for(uint32_t i = 0u; i < JOB_COUNT; i++) {
        // Reuse the job config of teh prev threads

        JS_ThreadPool_submit_jobs_with_parent(  &job_pool, 
                                                1u,
                                                &(JS_sJobConfig) {
                                                    .job_func = &Job_sum_func,
                                                    .read_only_data = (void*) &params[i],
                                                    .read_write_data = (void*) results,
                                                }, (JS_sJobConfig) {
                                                    .job_func = &Job_sum_func,
                                                    .read_only_data = (void*) &params[i],
                                                    .read_write_data = (void*) results,
                                                });
    }

    TIME_BLOCK( "Test 2 multithreaded",
                JS_ThreadPool_launch(&job_pool);
                JS_ThreadPool_wait_for(&job_pool)
    );

    result = 0u;

    for(uint32_t i = 0u; i < JOB_COUNT; i++) {
        result += results[i];
    }

    printf("Test 2: Result: %d / %d from %d jobs in %d threads\n", result, ARRAY_COUNT * 2u, JOB_COUNT, THREAD_COUNT);

    TIME_BLOCK( "Test 2 secuential",
        for(uint32_t i = 0u; i < JOB_COUNT; i++) {
            Job_sum_func(&params[i], results, NULL, 0);
            Job_sum_func(&params[i], results, NULL, 0);
        }
    );

    JS_ThreadPool_clean(&job_pool);

    return 0;
}