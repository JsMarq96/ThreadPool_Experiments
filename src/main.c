#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "thread_pool.h"

// TODO: get this programatically
#define THREAD_COUNT 8u
#define ARRAY_COUNT (THREAD_COUNT * 1000u * 10u)
#define JOB_COUNT (ARRAY_COUNT / 1000u)

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
    for(uint32_t i = 0u; i < 1000u; i++) {
        count += values[starting_idx + i];
    }

    ((uint32_t*)read_write)[params->write_to_idx] = count;
}

int main() {
    uint32_t base_values[ARRAY_COUNT];
    uint32_t results[JOB_COUNT];
    sJobParams params[JOB_COUNT];

    // Prepare the problem
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
            .startin_idx = i * 1000u,
            .values = base_values,
        };

        JS_ThreadPool_submit_job(&job_pool, 
                                (JS_sJobConfig) {
                                    .has_parent = false,
                                    .job_func = &Job_sum_func,
                                    .read_only_data = (void*) &params[i],
                                    .read_write_data = (void*) results,
                                    .parent_job_config = NULL,
                                });
    }

    JS_ThreadPool_launch(&job_pool);
    JS_ThreadPool_wait_for(&job_pool);

    uint32_t result = 0u;

    for(uint32_t i = 0u; i < JOB_COUNT; i++) {
        result += results[i];
    }

    printf("Result: %d / %d from %d jobs in %d threads\n", result, ARRAY_COUNT, JOB_COUNT, THREAD_COUNT);

    JS_ThreadPool_clean(&job_pool);

    return 0;
}