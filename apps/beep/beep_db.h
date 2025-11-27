#ifndef BEEP_DB_H
#define BEEP_DB_H

#include <stddef.h>
#include <stdint.h>
#include <meta_common.h>
#include <tcp_client.h>

#include <beep_datatypes.h>

// Going old-school here, ditching flatcc, xdr, and what else.
// We can always change our minds later.
// Note that this file is the truth file!
// boa@20251122
//

// Now let us add a new entity, Posts. We want to evaluate 1:n relations
typedef struct post_tag {
    dbid_t id;
    dbid_t user_id;
    text_t text;
} Post;

// So we have one Post, but what's many Posts?
// Array? Linked list? That's what we're trying to find out. If we lean into
// JSON syntax, we have something like [ Post{0,n} ] on the wire. When we
// read that in the client, we need to deal with n entries. We need a 
// container. list/dynamic array/realloc() array/. We care about clear code
// and good error handling, and reusability.

//int post_add(User, text_t text);
//int user_get_posts(User, Post);

#endif
