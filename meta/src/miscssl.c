#include <stdlib.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "miscssl.h"

/* This array will store all of the mutexes available to OpenSSL. */
static pthread_mutex_t *mutex_buf;

static void locking_function(int mode, int n, const char *file, int line)
{
    (void)file;
    (void)line;

    if (mode & CRYPTO_LOCK)
        pthread_mutex_lock(&mutex_buf[n]);
    else
        pthread_mutex_unlock(&mutex_buf[n]);
}

static unsigned long id_function(void)
{
    return (pthread_self());
}

struct CRYPTO_dynlock_value
{
    pthread_mutex_t mutex;
};

static struct CRYPTO_dynlock_value* dyn_create_function(const char *file,
    int line)
{
    struct CRYPTO_dynlock_value *p;

    (void)file;
    (void)line;

    p = malloc(sizeof *p);
    if (!p)
        return NULL;

    pthread_mutex_init(&p->mutex, NULL);
    return p;
}

static void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *p,
    const char *file, int line)
{
    (void)file;
    (void)line;

    if (mode & CRYPTO_LOCK)
        pthread_mutex_lock(&p->mutex);
    else
        pthread_mutex_unlock(&p->mutex);
}

static void dyn_destroy_function(struct CRYPTO_dynlock_value *p,
    const char *file, int line)
{
    (void)file;
    (void)line;

    pthread_mutex_destroy(&p->mutex);
    free(p);
}

status_t openssl_thread_setup(void)
{
    size_t i, n;

    n = CRYPTO_num_locks();
    mutex_buf = malloc(n * sizeof *mutex_buf);
    if (!mutex_buf)
        return failure;

    for (i = 0; i < n; i++)
        pthread_mutex_init(&mutex_buf[i], NULL);

    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);

    // The following three CRYPTO_... functions are the OpenSSL
    // functions for registering the callbacks we implemented above
    // 20200709: These fuckers expand to nothing. 
    // TODO: Re-evaluate how ssl does this irl
    CRYPTO_set_dynlock_create_callback(dyn_create_function);
    CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
    CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

    (void)id_function;
    (void)locking_function;
    (void)dyn_create_function;
    (void)dyn_lock_function;
    (void)dyn_destroy_function;

    return success;
}

status_t openssl_thread_cleanup(void)
{
    size_t i, n;
    if (mutex_buf == NULL)
        return success;

    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    CRYPTO_set_dynlock_create_callback(NULL);
    CRYPTO_set_dynlock_lock_callback(NULL);
    CRYPTO_set_dynlock_destroy_callback(NULL);

    n = CRYPTO_num_locks();
    for (i = 0; i < n; i++)
        pthread_mutex_destroy(&mutex_buf[i]);

    free(mutex_buf);
    mutex_buf = NULL;
    return success;
}

status_t openssl_init(void)
{
    if (!openssl_thread_setup() || !SSL_library_init())
        return failure;

    SSL_load_error_strings();
    ERR_load_crypto_strings();

    RAND_load_file("/dev/random", 1024);
    return success;
}
