#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <rfc1738.h>

/* Local helper 
 * Returns 1 on success, 0 on illegal input.
 */
static int encode(int c, char* dest)
{
    int high, low;

	/* Don't encode illegal input */
	if(c < 0 || c > 255)
		return 0;

    high = (c & 0xf0) >> 4;
    low = (c & 0x0f);

    if(high > 9) 
        high = 'A' + high - 10;
    else
        high = '0' + high;

    if(low > 9)
        low = 'A' + low - 10;
    else
        low = '0' + low;

    *dest++ = '%';
    *dest++ = high;
    *dest = low;
	return 1;
}

/* Local helper.
 * Accepts an alphanumeric character in the range [0-9a-fA-F] 
 * and returns it converted to an integer in the range 0-15. 
 */
static int hexchar2int(int c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    else if(c >= 'a' && c <= 'f') 
        return 10 + c - 'a';
    else if(c >= 'A' && c <= 'F') 
        return 10 + c - 'A';
    else /* Illegal input. */
        return -1;
}
    
/* Local helper.
 * Decodes an encoded character. Returns -1 on errors, else an 
 * integer which represents a decoded character. Note that hexchar2int
 * will implicitly detect end of string ('\0').
 */
static int decode(const char* src)
{
    int c1, c2;

	if(*src != '%')
		return -1;

    c1 = hexchar2int((unsigned char)*++src); 
    if(c1 == -1) 
        return -1;

    c2 = hexchar2int((unsigned char)*++src);
    if(c2 == -1)
        return -1;

    return (c1 << 4) + c2;
}

size_t rfc1738_encode(char* dest, size_t cbdest, const char* src, size_t cbsrc)
{
	size_t size = 0;

    assert(src != NULL);
    assert(dest != NULL);
    assert(cbdest > 0);
    assert(cbsrc > 0);

    while(cbsrc > 0 && cbdest > 0) {

        /* No isalnum() due to locale */
        if((*src >= 'A' && *src <= 'Z')
        || (*src >= 'a' && *src <= 'z')
        || (*src >= '0' && *src <= '9')) {
            *dest++ = *src++;
			size++;
			cbsrc--;
			cbdest--;
        }
        else if(cbdest > 2) {
            if(!encode((unsigned char)*src, dest)) {
				errno = EINVAL;
				return 0;
			}

            dest+=3; 
            cbdest-=3;   
            src++;
			size += 3;
			cbsrc--;
        }
        else {
			errno = ENOSPC;
            return 0;
		}
    }

    /* If we ran out of buffer space */
    if(*src != '\0') {
		errno = ENOSPC;
        return 0;
	}

    *dest = '\0';
    return size;
}

size_t rfc1738_decode(char* dest, size_t cbdest, const char* src, size_t cbsrc)
{
    int c;
	size_t size = 0;

    assert(src != NULL);
    assert(dest != NULL);

    while(cbdest > 0 && cbsrc > 0) {
        if(*src == '%') {
            if( (c = decode(src)) == -1) {
                errno = EINVAL;
                return 0;
            }

            *dest++ = c;
            src += 3; 
			cbsrc-=2;
        }
        else 
            *dest++ = *src++;

		size++;
        cbdest--;
		cbsrc--;
    }

    if(cbdest == 0) {
        errno = ENOSPC;
		return 0;
	}

	*dest = '\0';
    return size;
}

size_t rfc1738_encode_string(char* dest, size_t cbdest, const char* src)
{
	size_t size;

	if( (size = rfc1738_encode(dest, cbdest, src, strlen(src))) == 0)
		return 0;
	else if(size == cbdest)
		return 0; /* No room for null character */
	else {
		dest[size] = '\0';
		return size;
	}
}

size_t rfc1738_decode_string(char* dest, size_t cbdest, const char* src)
{
	size_t size;

	if( (size = rfc1738_decode(dest, cbdest, src, strlen(src))) == 0)
		return 0;
	else if(size == cbdest)
		return 0; /* No room for null character */
	else {
		dest[size] = '\0';
		return size;
	}
}

#ifdef CHECK_RFC1738
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    char speedy1[2048];
    char speedy2[2048];
	char buf1[1024] = { '\0' }, buf2[1024] = { '\0' };
    clock_t start, stop;
    double duration;
	size_t cb;

    static const char*tests[] = {
        "זרו ֶ״ֵ",
        "a couple of spaces",
        "specials %%%%%%,,,,,,::.-_0=[]{}???+\\\"",
    };

    size_t i, nelem;

    nelem = sizeof(tests) / sizeof(tests[0]);
    for(i = 0; i < nelem; i++) {
        buf1[0] = '\0';
        buf2[0] = '\0';
        if( (cb = rfc1738_encode(buf1, sizeof buf1, tests[i], strlen(tests[i]))) == 0) {
            fprintf(stderr, "1.Could not encode %s\n", tests[i]);
        }
        else if( (cb = rfc1738_decode(buf2, sizeof buf2, buf1, cb)) == 0) {
            fprintf(stderr, "2.Could not decode %s\n", buf1);
        }
        else if(memcmp(buf2, tests[i], cb) != 0) {
            fprintf(stderr,
				"enc/dec operation yielded different result: %s != %s\n"
                "Enc == %s\n",
                tests[i], buf2, buf1);
        }
    }

	/* Enc/dec all char codes */
	for(i = 1; i < 256; i++) 
		buf1[i - 1] = (int)i;
	buf1[i - 1] = '\0';
	if(!rfc1738_encode(buf2, sizeof buf2, buf1, strlen(buf1)) ) {
		perror("encode");
		exit(1);
	}
	else if(!rfc1738_decode(buf1, sizeof buf1, buf2, strlen(buf2)) ) {
		perror("decode");
		exit(1);
	}

	/* some illegal input */
	if(rfc1738_decode(buf1, sizeof buf1, "%", 1))
		assert(0 && "Oops, accepted illegal input");

	if(rfc1738_decode(buf1, sizeof(buf1), "%5", 2))
		assert(0 && "Oops, accepted illegal input");

	if(rfc1738_decode(buf1, sizeof(buf1), "%5X", 3))
		assert(0 && "Oops, accepted illegal input");

	/* buffer too small */
	if(rfc1738_decode(buf1, 4, "hello, world", 12))
		assert(0 && "Oops, accepted illegal input");

    /* Performance */
    memset(speedy1, ' ', 128);
    speedy1[128] = '\0';
    nelem = 10 * 1000;
    start = clock();
    for(i = 0; i < nelem; i++) {
        rfc1738_encode(speedy2, sizeof speedy2, speedy1, strlen(speedy1));
        rfc1738_decode(speedy1, sizeof speedy1, speedy2, strlen(speedy2));
    }

    stop = clock();
    duration = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    fprintf(stderr,
        "%lu enc/decs done in %.04f seconds (%.02f elements per sec)\n",
        (unsigned long)nelem,
        duration,
        (double)nelem/duration);

    return 0;
}
#endif

