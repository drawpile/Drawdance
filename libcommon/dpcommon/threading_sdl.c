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
#include "common.h"
#include "conversions.h"
#include "threading.h"
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>


struct DP_ThreadRunArgs {
    DP_ThreadFn fn;
    void *data;
};

struct DP_TlsValue {
    void *value;
    void (*destructor)(void *);
};


DP_Mutex *DP_mutex_new(void)
{
    SDL_mutex *sdl_mutex = SDL_CreateMutex();
    if (sdl_mutex) {
        return (DP_Mutex *)sdl_mutex;
    }
    else {
        DP_error_set("Can't create mutex: %s", SDL_GetError());
        return NULL;
    }
}

void DP_mutex_free(DP_Mutex *mutex)
{
    SDL_mutex *sdl_mutex = (SDL_mutex *)mutex;
    if (sdl_mutex) {
        SDL_DestroyMutex(sdl_mutex);
    }
}

bool DP_mutex_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    if (SDL_LockMutex((SDL_mutex *)mutex) == 0) {
        return true;
    }
    else {
        DP_error_set("Can't lock mutex: %s", SDL_GetError());
        return false;
    }
}

DP_MutexResult DP_mutex_try_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    switch (SDL_LockMutex((SDL_mutex *)mutex)) {
    case 0:
        return DP_MUTEX_OK;
    case SDL_MUTEX_TIMEDOUT:
        return DP_MUTEX_BLOCKED;
    default:
        DP_error_set("Can't try lock mutex: %s", SDL_GetError());
        return DP_MUTEX_ERROR;
    }
}

bool DP_mutex_unlock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    if (SDL_UnlockMutex((SDL_mutex *)mutex) == 0) {
        return true;
    }
    else {
        DP_error_set("Can't unlock mutex: %s", SDL_GetError());
        return false;
    }
}


DP_Semaphore *DP_semaphore_new(unsigned int initial_value)
{
    SDL_sem *sdl_sem = SDL_CreateSemaphore(DP_uint_to_uint32(initial_value));
    if (sdl_sem) {
        return (DP_Semaphore *)sdl_sem;
    }
    else {
        DP_error_set("Can't create semaphore: %s", SDL_GetError());
        return NULL;
    }
}

void DP_semaphore_free(DP_Semaphore *sem)
{
    SDL_sem *sdl_sem = (SDL_sem *)sem;
    if (sdl_sem) {
        SDL_DestroySemaphore(sdl_sem);
    }
}

bool DP_semaphore_post(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    if (SDL_SemPost((SDL_sem *)sem) == 0) {
        return true;
    }
    else {
        DP_error_set("Can't post semaphore: %s", SDL_GetError());
        return false;
    }
}

DP_SemaphoreResult DP_semaphore_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    switch (SDL_SemWait((SDL_sem *)sem)) {
    case 0:
        return DP_SEMAPHORE_OK;
    case SDL_MUTEX_TIMEDOUT:
        return DP_SEMAPHORE_INTERRUPTED;
    default:
        DP_error_set("Can't wait for semaphore: %s", SDL_GetError());
        return DP_SEMAPHORE_ERROR;
    }
}

DP_SemaphoreResult DP_semaphore_try_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    switch (SDL_SemTryWait((SDL_sem *)sem)) {
    case 0:
        return DP_SEMAPHORE_OK;
    case SDL_MUTEX_TIMEDOUT:
        return DP_SEMAPHORE_BLOCKED;
    default:
        DP_error_set("Can't try wait for semaphore: %s", SDL_GetError());
        return DP_SEMAPHORE_ERROR;
    }
}


static int SDLCALL run_thread(void *arg)
{
    struct DP_ThreadRunArgs *run_args = arg;
    DP_ThreadFn fn = run_args->fn;
    void *data = run_args->data;
    DP_free(run_args);
    fn(data);
    return 0;
}

DP_Thread *DP_thread_new(DP_ThreadFn fn, void *data)
{
    struct DP_ThreadRunArgs *run_args = DP_malloc(sizeof(*run_args));
    *run_args = (struct DP_ThreadRunArgs){fn, data};
    SDL_Thread *sdl_thread = SDL_CreateThread(run_thread, NULL, run_args);
    if (sdl_thread) {
        return (DP_Thread *)sdl_thread;
    }
    else {
        DP_free(run_args);
        DP_error_set("Error creating thread: %s", SDL_GetError());
        return NULL;
    }
}

void DP_thread_free_join(DP_Thread *thread)
{
    SDL_WaitThread((SDL_Thread *)thread, NULL);
}


static void SDLCALL tls_destroy(void *data)
{
    struct DP_TlsValue *tv = data;
    void (*destructor)(void *) = tv->destructor;
    if (destructor) {
        destructor(tv->value);
    }
    DP_free(tv);
}

DP_TlsKey DP_tls_create(void (*destructor)(void *))
{
    SDL_TLSID id = SDL_TLSCreate();
    if (id == DP_TLS_UNDEFINED) {
        DP_panic("Error creating thread-local key: %s", SDL_GetError());
    }

    struct DP_TlsValue *tv = DP_malloc(sizeof(*tv));
    *tv = (struct DP_TlsValue){NULL, destructor};
    if (SDL_TLSSet(id, tv, tls_destroy) != 0) {
        DP_panic("Error initializing thread-local key: %s", SDL_GetError());
    }

    return id;
}

void *DP_tls_get(DP_TlsKey key)
{
    DP_ASSERT(key != DP_TLS_UNDEFINED);
    SDL_TLSID id = key;
    struct DP_TlsValue *tv = SDL_TLSGet(id);
    if (tv) {
        return tv->value;
    }
    else {
        DP_panic("Error getting thread-local %u: %s", key, SDL_GetError());
    }
}

void DP_tls_set(DP_TlsKey key, void *value)
{
    DP_ASSERT(key != DP_TLS_UNDEFINED);
    SDL_TLSID id = key;
    struct DP_TlsValue *tv = SDL_TLSGet(id);
    if (tv) {
        tv->value = value;
    }
    else {
        DP_panic("Error setting thread-local %u = %p: %s", key, value,
                 SDL_GetError());
    }
}
