#ifndef BEEP_CONSTANTS_H
#define BEEP_CONSTANTS_H

// Some constants
#define NAME_MAX 50
#define EMAIL_MAX 500
#define NICK_MAX NAME_MAX
#define TEXT_MAX 1023

// FIRST OF ALL, so we never forget: Message IDs. Each server request
// has an ID which must be unique and will be used by the dispatcher
// to locate the handler function.
//
// These constants come to the server via the request header. They
// MUST be unique so parsing of payload works as expected.
#define BEEP_USER_ADD 0x01
#define BEEP_USER_DEL 0x02
#define BEEP_USER_UPD 0x03
#define BEEP_USER_GET 0x04



#endif

