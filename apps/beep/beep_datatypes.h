#ifndef BEEP_DATATYPES_H
#define BEEP_DATATYPES_H

#include <stdint.h>
#include <beep_constants.h>

// Some datatypes
typedef uint64_t dbid_t;
typedef int64_t timestamp;
typedef char name_t[NAME_MAX + 1];
typedef char email_t[EMAIL_MAX + 1];
typedef char nick_t[NICK_MAX + 1];
typedef char text_t[TEXT_MAX + 1];

#endif

