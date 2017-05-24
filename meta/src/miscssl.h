#ifndef META_MISCSSL_H
#define META_MISCSSL_H

#include <meta_common.h>

status_t openssl_thread_setup(void) __attribute__((warn_unused_result));
status_t openssl_thread_cleanup(void) __attribute__((warn_unused_result));
status_t openssl_init(void) __attribute__((warn_unused_result));

#endif
