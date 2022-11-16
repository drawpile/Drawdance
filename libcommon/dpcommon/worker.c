/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "worker.h"
#include "common.h"
#include "conversions.h"
#include "queue.h"
#include "threading.h"


#define ELEMENT_SIZE sizeof(DP_WorkerJob)

typedef struct DP_WorkerJob {
    DP_WorkerFn fn;
    void *user;
} DP_WorkerJob;

typedef struct DP_Worker {
    DP_Semaphore *sem;
    DP_Queue queue;
    DP_Mutex *queue_mutex;
    int thread_count;
    DP_Thread *threads[];
} DP_Worker;


static DP_WorkerFn shift_worker_job(DP_Mutex *queue_mutex, DP_Queue *queue,
                                    void **out_user)
{
    DP_MUTEX_MUST_LOCK(queue_mutex);
    DP_WorkerJob *job = DP_queue_peek(queue, ELEMENT_SIZE);
    DP_WorkerFn fn;
    if (job) {
        fn = job->fn;
        *out_user = job->user;
        DP_queue_shift(queue);
    }
    else {
        fn = NULL;
    }
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
    return fn;
}

static void run_worker_thread(void *data)
{
    DP_Worker *worker = data;
    DP_Semaphore *sem = worker->sem;
    DP_Mutex *queue_mutex = worker->queue_mutex;
    DP_Queue *queue = &worker->queue;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        void *user;
        DP_WorkerFn fn = shift_worker_job(queue_mutex, queue, &user);
        if (fn) {
            fn(user);
        }
        else {
            break;
        }
    }
}

DP_Worker *DP_worker_new(size_t initial_capacity, int thread_count)
{
    DP_ASSERT(initial_capacity > 0);
    DP_ASSERT(thread_count > 0);
    DP_Worker *worker = DP_malloc_zeroed(
        DP_FLEX_SIZEOF(DP_Worker, threads, DP_int_to_size(thread_count)));

    worker->sem = DP_semaphore_new(0);
    if (!worker->sem) {
        DP_worker_free_join(worker);
        return NULL;
    }

    DP_queue_init(&worker->queue, initial_capacity, ELEMENT_SIZE);

    worker->queue_mutex = DP_mutex_new();
    if (!worker->queue_mutex) {
        DP_worker_free_join(worker);
        return NULL;
    }

    for (int i = 0; i < thread_count; ++i) {
        DP_Thread *thread = DP_thread_new(run_worker_thread, worker);
        if (thread) {
            worker->threads[i] = thread;
            worker->thread_count = i + 1;
        }
        else {
            DP_worker_free_join(worker);
            return NULL;
        }
    }

    return worker;
}

void DP_worker_free_join(DP_Worker *worker)
{
    if (worker) {
        DP_Semaphore *sem = worker->sem;
        int thread_count = worker->thread_count;
        if (sem) {
            DP_SEMAPHORE_MUST_POST_N(sem, thread_count);
        }
        for (int i = 0; i < thread_count; ++i) {
            DP_thread_free_join(worker->threads[i]);
        }
        DP_mutex_free(worker->queue_mutex);
        DP_queue_dispose(&worker->queue);
        DP_semaphore_free(sem);
        DP_free(worker);
    }
}

void DP_worker_push(DP_Worker *worker, DP_WorkerFn fn, void *user)
{
    DP_ASSERT(worker);
    DP_ASSERT(fn);
    DP_Mutex *queue_mutex = worker->queue_mutex;
    DP_MUTEX_MUST_LOCK(queue_mutex);
    DP_WorkerJob *job = DP_queue_push(&worker->queue, ELEMENT_SIZE);
    *job = (DP_WorkerJob){fn, user};
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
    DP_SEMAPHORE_MUST_POST(worker->sem);
}
