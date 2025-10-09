#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "miscssl.h"

status_t openssl_init(void)
{
    if (!OPENSSL_init_ssl(0, NULL))
        return failure;

    SSL_load_error_strings();
    ERR_load_crypto_strings();

    return success;
}

status_t openssl_exit(void)
{
    OPENSSL_cleanup();
    return success;
}
