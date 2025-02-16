#ifndef __THREADS_INCLUDE_H__
#define __THREADS_INCLUDE_H__

/**
 * Including either threads or tinyctreads
 * By Juan S. Marquerie (JsMarq96)
 */

#ifdef __STDC_NO_THREADS__
    #define USING_TINY_C_THREADS 
    #include <tinycthread.h>
#else
    #define USING_C11_THREADS 
    #include <threads.h>
#endif // __STDC_NO_THREADS__

#endif // __THREADS_INCLUDE_H__