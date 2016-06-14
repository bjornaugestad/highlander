#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include <meta_bignum.h>

#if defined(BIGNUM_CHECK) || defined(BIGNUM_SPEED)

static void dump(const bignum *p, const char *lead)
{
	size_t i = sizeof p->value / sizeof *p->value;
    size_t nzeros = 0;

    while (nzeros < i && p->value[nzeros] == 0)
        nzeros++;

	fprintf(stderr, "%s\n", lead);
    if (nzeros == i)
        printf("\t0\n");

	while (i-- > nzeros)
		fprintf(stderr, "\t%zu:%llx\n", i, (unsigned long long)p->value[i]);

}
#endif

static inline void bignum_zero(bignum *p)
{
    assert(p != NULL);

#if 1
    memset(p->value, 0, sizeof p->value);

#else
    size_t i, n = sizeof p->value / sizeof *p->value;

    for (i = 0; i < n; i += 8) {
        p->value[i] = 0;
        p->value[i + 1] = 0;
        p->value[i + 2] = 0;
        p->value[i + 3] = 0;
        p->value[i + 4] = 0;
        p->value[i + 5] = 0;
        p->value[i + 6] = 0;
        p->value[i + 7] = 0;
    }
#endif
}

bignum* bignum_new(const char *value)
{
	bignum *p;

	assert(value != NULL);
	assert(valid_bignum(value));

	if ((p = malloc(sizeof *p)) == NULL)
		return NULL;

	if (!bignum_set(p, value)) {
        free(p);
        return NULL;
    }

	return p;
}

void bignum_free(bignum *p)
{
	free(p);
}

// Both a and b have fixed sizes. Just loop and add.
// We need a 128 bit intermediate. Does that suck? Maybe.
static inline status_t add(bignum * restrict dest, const bignum *a, const bignum *b)
{
	size_t i;
	uint64_t carry = 0;
    unsigned __int128 sum;

	// We iterate from right to left
	i = sizeof a->value / sizeof *a->value;

    bignum_zero(dest);

	while(i--) {
        // Step by step additions to avoid overflow on intermediate values
		sum = a->value[i];
        sum += b->value[i];
        sum += carry;
#if 0
		carry = sum > UINT64_MAX;
#else
        // debugging
        if (sum > UINT64_MAX)
            carry = 1;
        else 
            carry = 0;
#endif
		dest->value[i] = sum;
	}

	if (carry)
        return failure; // Overflow.

	return success;
}

// aaa - add and assign rhs to lhs, as in lhs += rhs.
// How does this work? For each digit in rhs, add it to lhs.
// Handle carry properly
static inline status_t aaa(bignum * restrict lhs, const bignum *rhs)
{
    bignum tmp;

    if (!bignum_set(&tmp, ""))
        die("Could not set value");
    if (!bignum_add(&tmp, lhs, rhs))
        return failure;
    *lhs = tmp;
    return success;
}

status_t bignum_add(bignum * restrict dest, const bignum *a, const bignum *b)
{
	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);
    assert(dest != a); // We don't like aliases
    assert(dest != b);

    bignum_zero(dest);

    return add(dest, a, b);
}

status_t bignum_sub(bignum *dest, const bignum *a, const bignum *b)
{
#if 1
    (void)dest;(void)a;(void)b;
#else
	size_t i;
	int borrow = 0;
    signed long long delta;

	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

	i = sizeof a->value / sizeof *a->value;
    bignum_zero(dest);

	while (i--) {
		delta = a->value[i] - b->value[i] - borrow;
		borrow = delta < 0;
		dest->value[i] = delta;
	}

	if (borrow)
		return failure;
#endif

	return success;
}

// Left-shift one bit. Return error on overflow
status_t bignum_lshift(bignum *p)
{
	size_t i;
	unsigned char prev = 0, overflow = 0;

	assert(p != NULL);

	// we iterate from LSB to MSB
	i = sizeof p->value / sizeof *p->value;
	while (i--) {
		overflow = p->value[i] & 0x8000000000000000 ? 1 : 0;
		p->value[i] <<= 1;
		p->value[i] |= prev;
		prev = overflow;
	}

    if (overflow)
        return failure;

    return success;
}

status_t bignum_mul(bignum *dest, const bignum *a, const bignum *b)
{
	//unsigned char mask;
	//size_t i;
    //bignum aa; // We need a writable(shiftable) copy of a

	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

    //aa = *a;
    bignum_zero(dest);

#if 0

    for (i = 0; i < b->len; i++) {
        for (mask = 1; mask; mask <<= 1) {
            if (mask & b->value[sizeof b->value - i])
                aaa(dest, &aa);

            bignum_lshift(&aa);
        }
    }
#endif
	return success;
}

status_t bignum_div(bignum *dest, const bignum *a, const bignum *b)
{
	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

	(void)dest; (void)a; (void)b;
	return failure;
}

status_t bignum_mod(bignum *dest, const bignum *a, const bignum *b)
{
	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

	(void)dest; (void)a; (void)b;
	return failure;
}

status_t bignum_divmod(bignum *quot, bignum *rem, const bignum *a, const bignum *b)
{
	assert(quot != NULL);
	assert(rem != NULL);
	assert(a != NULL);
	assert(b != NULL);

	(void)quot; (void)rem; (void)a; (void)b;
	return failure;
}

static inline unsigned char tohex(char c)
{
	assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));

	if (c >= 'c' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return c - '0';
}

status_t bignum_set(bignum *p, const char *value)
{
	size_t i, j, n;

	assert(p != NULL);
	assert(value != NULL);
	assert(valid_bignum(value));

    bignum_zero(p);

	n = strlen(value);
    assert(n % 2 == 0 && "No odd-length values");

	i = sizeof p->value / sizeof *p->value;

    // Is there room for the value in bignum?
    // value is hex, so we use 4 bits per byte.
    // p->value can store i * sizeof *p->value bytes.
    if (n > sizeof p->value * 2)
        return failure;

	// Consume 16 bytes from 'value' for each iteration.
	// Remember that we store values as MSB, so 
	// lower bits go to the end of the array.
    // Also keep in mind that i refers to uint64_t array entries,
    // and n to char entries. Each iteration decrements n with 8
    // and i with 1.
	while(n > sizeof *p->value * 2 && i--) {
        n--;
		p->value[i] = tohex(value[n - 15]) << 4;
		p->value[i] |= tohex(value[n - 14]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 13]) << 4;
		p->value[i] |= tohex(value[n - 12]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 11]) << 4;
		p->value[i] |= tohex(value[n - 10]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 9]) << 4;
		p->value[i] |= tohex(value[n - 8]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 7]) << 4;
		p->value[i] |= tohex(value[n - 6]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 5]) << 4;
		p->value[i] |= tohex(value[n - 4]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 3]) << 4;
		p->value[i] |= tohex(value[n - 2]);
        p->value[i] <<= 8;

		p->value[i] |= tohex(value[n - 1]) << 4;
		p->value[i] |= tohex(value[n]);
        n -= 15;
	}

    // Now n <= 7. Add the last entries, but stop if n == 0.
    if (n == 0)
        return success;

    i--;
    for (j = 0; j < n; j += 2) {
		p->value[i] |= tohex(value[j]) << 4;
		p->value[i] |= tohex(value[j + 1]);
        if (j + 2 < n)
            p->value[i] <<= 8;
    }

    return success;
}

bool valid_bignum(const char *value)
{
	size_t i, n;
    bignum *dummy;

	assert(value != NULL);

	n = strlen(value);
	if (n % 2 != 0)
		return false;

	if (n > sizeof dummy->value * 2)
		return false;

	for (i = 0; i < n; i++) {
		if (!isxdigit(value[i]))
			return false;
	}

	return true;
}

int bignum_cmp(const bignum *a, const bignum *b)
{
	assert(a != NULL);
	assert(b != NULL);

	return memcmp(a->value, b->value, sizeof a->value);
}

#if defined(BIGNUM_CHECK) || defined(BIGNUM_SPEED)

static const char maxval[1025] =
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
;

static const char halfval[1025] =
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
;
#endif

#ifdef BIGNUM_SPEED
int main(void)
{
	size_t i, niter = 1000 * 1000;
	bignum max, zero, dest, half;

	if (!bignum_set(&max, maxval))
        die("Could not set value");
	if (!bignum_set(&half, halfval))
        die("Could not set value");
	if (!bignum_set(&zero, ""))
        die("Could not set value");

	for (i = 0; i < niter; i++)
		bignum_add(&dest, &max, &zero);

	for (i = 0; i < niter; i++) {
		if (!bignum_add(&dest, &half, &half))
			return 1;
	}
	
	if (0) dump(&dest, "dest");

	return 0;
}
#endif

#ifdef BIGNUM_CHECK

static void check_sub(void)
{
    return;

	bignum a, b, c, facit;

	if (!bignum_set(&a, "ff"))
        die("Could not set value");
	if (!bignum_set(&b, "01"))
        die("Could not set value");
	if (!bignum_sub(&c, &a, &b))
		exit(2);

	if (!bignum_set(&facit, "fe"))
        die("Could not set value");
	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been fe");
		exit(3);
	}

	if (!bignum_set(&facit, ""))
        die("Could not set value");
	if (!bignum_set(&a, maxval))
        die("Could not set value");
	if (!bignum_sub(&c, &a, &a))
		exit(4);

	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been zero\n");
		exit(5);
	}

	if (!bignum_set(&b, halfval))
        die("Could not set value");
	if (!bignum_sub(&c, &a, &b))
		exit(6);

	if (!bignum_set(&a, ""))
        die("Could not set value");
	if (!bignum_set(&b, ""))
        die("Could not set value");
	if (!bignum_set(&facit, ""))
        die("Could not set value");
	if (!bignum_sub(&c, &a, &b))
		exit(7);

	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been zero\n");
		exit(8);
	}

	if (!bignum_set(&a, "ff00"))
        die("Could not set value");
	if (!bignum_set(&b, "01"))
        die("Could not set value");
	if (!bignum_set(&facit, "feff"))
        die("Could not set value");
	if (!bignum_sub(&c, &a, &b))
		exit(6);

	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been 0xfeff\n");
		exit(8);
	}
}

static void check_lshift(void)
{
	bignum a, b;
	size_t i;

	if (!bignum_set(&a, "01"))
        die("Could not set value");
	if (!bignum_set(&b, "02"))
        die("Could not set value");

	bignum_lshift(&a);
	if (bignum_cmp(&a, &b) != 0) {
		dump(&a, "Should've been 02");
		exit(10);
	}

	if (!bignum_set(&a, "01"))
        die("Could not set value");
	if (!bignum_set(&b, "80000000"))
        die("Could not set value");

	for (i = 0; i < 31; i++)
		bignum_lshift(&a);

	if (bignum_cmp(&a, &b) != 0) {
		dump(&a, "Should've been b");
		dump(&b, "b");
		exit(11);
	}
}

static void check_aaa(void)
{
    size_t i;
    bignum lhs, rhs, facit;

    if (!bignum_set(&lhs, ""))
        die("Could not set value");
    if (!bignum_set(&rhs, "01"))
        die("Could not set value");

    for (i = 0; i < 1024; i++) {
        if (!aaa(&lhs, &rhs))
            die("aaa() failed\n");
    }

    if (!bignum_set(&facit, "0400"))
        die("Could not set value");
    if (bignum_cmp(&facit, &lhs)) {
        dump(&lhs, "Expected ff");
        dump(&facit, "facit");
        exit(11);
    }
}

static void check_mul(void)
{
    static const struct {
        const char *op1, *op2, *facit;
    } tests[] = {
        { "ff", "01", "ff" },
        { "ff", "02", "01fe" },
        { "ff", "ff", "fe01" }
    };

    size_t i, n = sizeof tests / sizeof *tests;
    bignum res, op1, op2, facit;

    for (i = 0; i < n; i++) {
        if (!bignum_set(&op1, tests[i].op1))
            die("Could not set value");

        if (!bignum_set(&op2, tests[i].op2))
            die("Could not set value");

        if (!bignum_set(&facit, tests[i].facit))
            die("Could not set value");

        if (!bignum_mul(&res, &op1, &op2))
            die("Unable to multiply...");

        if (bignum_cmp(&res, &facit)) {
            fprintf(stderr, "Error multiplying %s with %s.\n",
                tests[i].op1, tests[i].op2);
            dump(&res, "Result");
            dump(&facit, "Expected");
            exit(12);
        }
    }
}

static void check_alloc(void)
{
	bignum *p;

	p = bignum_new("ffaabbcc");
	if (p == NULL)
		die("Out of memory or illegal input");

	if (!bignum_set(p, "deadbabe"))
        die("Could not set value");

	bignum_free(p);
}


static void check_add(void)
{
	bignum a, b, c, facit;

	if (!bignum_set(&a, "ff") || !bignum_set(&b, ""))
        die("Could not set value");

	if (bignum_cmp(&a, &b) <= 0) {
        dump(&a, "a");
        dump(&b, "b");
		die("a >= b");
    }

	if (bignum_cmp(&b, &a) >= 0)
		die("2");

	if (bignum_cmp(&a, &a) != 0)
		die("3");

	if (!bignum_add(&c, &a, &b))
		die("4");

	if (bignum_cmp(&c, &a) != 0) {
        dump(&a, "a");
        dump(&c, "c");
		die("5: ff plus 0 was not ff.");
    }

	if (!bignum_set(&a, "ff") || !bignum_set(&b, "01"))
        die("Could not set value");

	if (!bignum_add(&c, &a, &b))
		die("Could not add\n");

	if (!bignum_set(&facit, "0100"))
        die("Could not set value");

	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "c");
		dump(&facit, "Facit");
		die("c != facit");
	}

	// We can add 0 to maxval and get maxval.
	if (!bignum_set(&a, maxval))
        die("Could not set value");
	if (!bignum_set(&b, "00"))
        die("Could not set value");
	if (!bignum_add(&c, &a, &b))
		die("Could not add a and b");

	// We can not add 1 to maxval.
	if (!bignum_set(&a, maxval))
        die("Could not set value");
	if (!bignum_set(&b, "01"))
        die("Could not set value");

	if (bignum_add(&c, &a, &b))
		die("Could add a and b, should've been overflow");

	if (bignum_add(&c, &b, &a))
		die("Could add b and a, should've been overflow");

}

int main(void)
{
	const char *value = "cafebabedeadbeef";
	const char *odd_len1 = "f"; // We need a multiple of 2.
	const char *odd_len3 = "fad"; // We need a multiple of 2.
	const char *invalid_chars = "foobar";
    bignum *dummy;

    check_add();
	check_sub();
	check_lshift();
    check_aaa();
    check_mul();
    check_alloc();

	// Empty strings == zero.
	if (!valid_bignum(""))
		die("X1");

	if (!valid_bignum(maxval))
		die("X2");

	if (!valid_bignum(value))
		die("X3");

	if (valid_bignum(invalid_chars))
		die("X4");

	if (valid_bignum(odd_len1))
		die("X5");

	if (valid_bignum(odd_len3))
		die("X6");

	char too_long_value[sizeof *dummy->value * 2 + 4];
	memset(too_long_value, 'a', sizeof too_long_value);
	too_long_value[sizeof too_long_value - 1] = '\0';
	if (valid_bignum(too_long_value))
		die("X7");

	return 0;
}

#endif
