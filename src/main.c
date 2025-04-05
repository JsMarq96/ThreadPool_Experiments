#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "threads_include.h"
#include "thread_pool.h"

// TODO: get this programatically
#define THREAD_COUNT 8u
#define TO_DO_PER_JOB 2000u
#define ARRAY_COUNT (10u * TO_DO_PER_JOB * 100u)
#define JOB_COUNT (ARRAY_COUNT / TO_DO_PER_JOB)

#ifdef _WIN32
    #define TIME_BLOCK(label, block_to_eval) {\
        LARGE_INTEGER counter_freq, start_time, end_time;\
        QueryPerformanceFrequency(&counter_freq);\
        QueryPerformanceCounter(&start_time);\
        block_to_eval;\
        QueryPerformanceCounter(&end_time);\
        printf("%s: %f ms\n",label, (end_time.QuadPart-start_time.QuadPart) / ((double) counter_freq.QuadPart/1000.0f));\
    }
#else //  _WIN32
    #define GET_DOUBLE_MS_FROM_TIME(time_struct) ((double) time_struct.tv_sec * 1e9 + time_struct.tv_nsec)

    #define TIME_BLOCK(label, block_to_eval) {\
        struct timespec begin_time, end_time;\
        clock_gettime(CLOCK_REALTIME, &begin_time);\
        block_to_eval;\
        clock_gettime(CLOCK_REALTIME, &end_time);\
        double f = GET_DOUBLE_MS_FROM_TIME(end_time) - GET_DOUBLE_MS_FROM_TIME(begin_time); \
        printf("%s: %f ms\n",label, f/1000000); \
    }
#endif //  _WIN32

#define MATRIX_SIZE 1024u

typedef struct sJobParams {
    uint32_t row_idx;
} sJobParams;

uint32_t matrix_to_sum[MATRIX_SIZE * MATRIX_SIZE];
uint64_t tmp_values[MATRIX_SIZE];
uint64_t resuling_values = 0u;
sJobParams params[MATRIX_SIZE];

static void Job_sum_mat(const void* read_only, void* read_write, struct JS_sThreadPool* pool, const uint8_t thread_id) {
    const uint32_t row_idx = ((sJobParams*) read_only)->row_idx;

    uint64_t count = 0u;
    for(uint32_t i = 0u; i < MATRIX_SIZE; i++) {
        count += matrix_to_sum[i + row_idx * MATRIX_SIZE];
    }

    tmp_values[row_idx] = count;
}

static void Job_sum_mat2(const void* read_only, void* read_write, struct JS_sThreadPool* pool, const uint8_t thread_id) {
    uint64_t count = 0u;
    for(uint32_t i = 0u; i < MATRIX_SIZE; i++) {
        count += tmp_values[i];
    }

    resuling_values = count;
}



int main(void) {
    // Prepare the problem first problem
    for(uint32_t i = 0u; i < (MATRIX_SIZE * MATRIX_SIZE); i++) {
       matrix_to_sum[i] = 1u;
    }

    JS_sThreadPool job_pool;
    JS_ThreadPool_init(&job_pool, THREAD_COUNT);

    JS_sJobConfig child_jobs_config[MATRIX_SIZE] = {};

    for(uint32_t i = 0u; i < MATRIX_SIZE; i++) {
        params[i] = (sJobParams){
            .row_idx = i,
        };

        child_jobs_config[i] = (JS_sJobConfig) {
                                    .job_func = &Job_sum_mat,
                                    .read_only_data = (void*) &params[i],
                                    .read_write_data = (void*) NULL,
                                };
    }

    JS_ThreadPool_submit_jobs_with_parent(  &job_pool, 
                                            MATRIX_SIZE, 
                                            child_jobs_config, 
                                            (JS_sJobConfig) {
                                                .job_func = &Job_sum_mat2,
                                                .read_only_data = (void*) NULL,
                                                .read_write_data = (void*) NULL,
                                            });

    TIME_BLOCK( "Test 1 multithreaded",
                JS_ThreadPool_launch(&job_pool);
                JS_ThreadPool_wait_for(&job_pool););

    printf("Result: %u\n", resuling_values);

    uint64_t sum = 0u;
    TIME_BLOCK( "Test 2 singlethreaded",
                for(uint32_t i = 0u; i < (MATRIX_SIZE * MATRIX_SIZE); i++) {
                    sum += matrix_to_sum[i];
                });
    
    printf("Result: %u\n", sum);

    JS_ThreadPool_clean(&job_pool);

    return 0;
}