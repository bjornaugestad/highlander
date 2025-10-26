#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/provider.h>

#include "miscssl.h"

static OSSL_PROVIDER *g_provider;
status_t openssl_init(void)
{
    if (!OPENSSL_init_ssl(0, NULL))
        return failure;

    g_provider = OSSL_PROVIDER_load(NULL, "default");
    return success;
}

status_t openssl_exit(void)
{
    OSSL_PROVIDER_unload(g_provider);
    OPENSSL_cleanup();
    return success;
}
