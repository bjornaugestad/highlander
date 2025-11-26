#ifndef BEEP_SERIALIZER_H
#define BEEP_SERIALIZER_H

#include <meta_common.h>

// Big and important intro!
// The serialiser reads and writes data to and from a format suitable for
// transport over the network. It's a binary format with tags. It
// supports arrays and nested arrays too, just like JSON. 
//
// We want to transport C structs over the wire. Also, we want to transport
// lists and arrays which may be nested and eventually contain leaf values.
// That's why we mimic JSON's [] and {}. [] are arrays. {} are records.
//
// Real world example: Return an 1:n dataset as in "user has n posts". The number of 
// posts would be[, and the array entries would be the records in the array.
//
// We support datatypes explicitly with a one-character code. Here's a list of what we support and how:
// c = int8 (1B)
// C = uint8 (1B)
// h = int16 (2B)
// H = uint16 (2B)
// i = int32 (4B)
// I = uint32 (4B)
// l = int64 (8B)
// L = uint64 (8B)
// f = float32 (4B)
// d = float64/double (8B)
// D = datetime(8B)
// b = boolean(t = true, f = false)
// Z = null

// Varlen scalars (tag + varint-len + raw bytes):
// Q = UTF-8 string. Format = Q0xnnnnnnnn, as in 32 bit unsigned length indicator. Same goes for X.
// X = raw bytes (images/avatars/etc)

// Literals (tag alene, 0 payload):

// Containers:

// [ = array start, followed by varint count: [ + n + (n verdier) + ]
// ] = array end

// { = object/tuple start (posisjonelt eller key-value etter schema)
// } = object/tuple end
//
// As for byte order, it's tempting to use LBO on the wire for performance reasons. 
//
//
// We need two objects, an encoder and a decoder:
// The encoder places values in a buffer and manages size
// The decoder reads values from the buffer.
// Since the encoder literally is a byte buffer, we can create a generalized buffer
// elsewhere and just use that one. The buffer doesn't care about content.
// We already have the membuf, but it's not really suited for this task and cannot
// be extended on demand. We need that since we not always know the size.
// Hmm, that raises an interesting question for 1:n queries when n is unknown.
// We want to know n, but do we want to store the data in intermediate form
// just to be able to count n? I don't think so, bro.

    

status_t beep_get_i8(const char *src, size_t srclen, char *dest);
status_t beep_put_i8(char *dest, size_t destsize, char c);

#endif

