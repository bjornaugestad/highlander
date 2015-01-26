#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include <meta_bignum.h>

#if defined(BIGNUM_CHECK) || defined(BIGNUM_SPEED)

static void dump(const bignum *p, const char *lead)
{
	size_t i;

	fprintf(stderr, "%s\n", lead);
	fprintf(stderr, "\tlength: %zu\n", p->len);

	for (i = sizeof p->value - p->len; i < sizeof p->value; i++)
		fprintf(stderr, "\t%zu:%d\n", i, p->value[i]);

}
#endif

bignum* bignum_new(const char *value)
{
	bignum *p;

	assert(value != NULL);
	assert(valid_bignum(value));

	if ((p = malloc(sizeof *p)) == NULL)
		return NULL;

	bignum_set(p, value);
	return p;
}

void bignum_free(bignum *p)
{
	free(p);
}

// a must be longer than b for this to work.
static inline status_t add(bignum *dest, const bignum *a, const bignum *b)
{
	size_t ai, bi, an, bn;
	unsigned int sum, carry = 0;

	assert(a->len >= b->len);

	// We iterate from right to left
	ai = sizeof a->value - 1;
	bi = sizeof b->value - 1;
	an = a->len;
	bn = b->len;

	// First we loop for all cases where we have two values.
	while(bn--) {
		sum = a->value[ai] + b->value[bi] + carry;
		carry = sum > 0xff;
		dest->value[ai] = sum;
		dest->len++;
		ai--;
		bi--;
	}

	// Then we add the rest.
	an -= b->len;

	while (an--) {
		sum = a->value[ai] + carry;
		carry = sum > 0xff;
		dest->value[ai] = sum;
		dest->len++;
		ai--;
	}

	if (carry) {
		if (dest->len == sizeof dest->value)
			return failure; // Overflow.

		// Set carry
		dest->len++;
		dest->value[sizeof dest->value - dest->len] = 1;
	}

	return success;
}

status_t bignum_add(bignum *dest, const bignum *a, const bignum *b)
{
	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

	memset(dest->value, 0, sizeof dest->value);
	dest->len = 0;

	if (a->len >= b->len)
		return add(dest, a, b);
	else 
		return add(dest, b, a);
}

status_t bignum_sub(bignum *dest, const bignum *a, const bignum *b)
{
	size_t i, an, bn;
	int delta, borrow = 0;

	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

	an = a->len;
	bn = b->len;
	assert(an >= bn); // No support for wraparound

	i = sizeof a->value - 1;
	dest->len = 0;
	memset(dest->value, 0, sizeof dest->value);

	while (bn--) {
		delta = a->value[i] - b->value[i] - borrow;
		borrow = delta < 0;
		dest->value[i] = delta;
		dest->len++;
		i--;
	}

	// Now just carry the borrow
	an -= b->len;
	while(an--) {
		delta = a->value[i] - borrow;
		borrow = delta < 0;
		dest->value[i] = delta;
		dest->len++;
		i--;
	}

	if (borrow)
		return failure;

	// Reduce length to ignore leading zero bytes.
	for (i = sizeof dest->value - dest->len; i < sizeof dest->value; i++) {
		if (dest->value[i] != 0)
			break;

		if (dest->len == 0)
			break;

		dest->len--;
	}
		
	return success;
}

status_t bignum_mul(bignum *dest, const bignum *a, const bignum *b)
{
	assert(dest != NULL);
	assert(a != NULL);
	assert(b != NULL);

	(void)dest; (void)a; (void)b;
	return failure;
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

void bignum_set(bignum *p, const char *value)
{
	size_t i, n;

	assert(p != NULL);
	assert(value != NULL);
	assert(valid_bignum(value));

	memset(p->value, 0, sizeof p->value);

	n = strlen(value);
	p->len = n / 2;
	i = sizeof p->value - 1;

	// Consume two bytes for each iteration.
	// Remember that we store values as MSB, so 
	// lower bits go to the end of the array.
	while(n--) {
		p->value[i] = tohex(value[n--]);
		p->value[i] |= tohex(value[n]) << 4;
		i--;
	}

	assert(p->len == strlen(value) / 2);
}

bool valid_bignum(const char *value)
{
	size_t i, n;

	assert(value != NULL);

	n = strlen(value);
	if (n % 2 != 0)
		return false;

	if (n > META_BIGNUM_MAXBYTES * 2)
		return false;

	for (i = 0; i < n; i++) {
		if (!isxdigit(value[i]))
			return false;
	}

	return true;
}

int bignum_cmp(const bignum *a, const bignum *b)
{
	size_t offset;

	assert(a != NULL);
	assert(b != NULL);

	if (a->len > b->len)
		return -1;
	
	if (b->len > a->len)
		return 1;

	offset = sizeof a->value - a->len;
	return memcmp(a->value + offset, b->value + offset, a->len);
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

	bignum_set(&max, maxval);
	bignum_set(&half, halfval);
	bignum_set(&zero, "");

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
	bignum a, b, c, facit;

	bignum_set(&a, "ff");
	bignum_set(&b, "01");
	if (!bignum_sub(&c, &a, &b))
		exit(2);

	bignum_set(&facit, "fe");
	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been fe");
		exit(3);
	}

	bignum_set(&facit, "");
	bignum_set(&a, maxval);
	if (!bignum_sub(&c, &a, &a))
		exit(4);

	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been zero\n");
		exit(5);
	}

	bignum_set(&b, halfval);
	if (!bignum_sub(&c, &a, &b))
		exit(6);

	bignum_set(&a, "");
	bignum_set(&b, "");
	bignum_set(&facit, "");
	if (!bignum_sub(&c, &a, &b))
		exit(7);

	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "Should've been zero\n");
		exit(8);
	}

}

int main(void)
{
	bignum *p, a, b, c, facit;
	const char *value = "cafebabedeadbeef";
	const char *odd_len1 = "f"; // We need a multiple of 2.
	const char *odd_len3 = "fad"; // We need a multiple of 2.
	const char *invalid_chars = "foobar";
	char too_long_value[META_BIGNUM_MAXBYTES * 2 + 4];

	check_sub();

	// Empty strings == zero.
	if (!valid_bignum(""))
		return 1;

	if (!valid_bignum(maxval))
		return 1;

	if (!valid_bignum(value))
		return 1;

	if (valid_bignum(invalid_chars))
		return 1;

	if (valid_bignum(odd_len1))
		return 1;

	if (valid_bignum(odd_len3))
		return 1;

	memset(too_long_value, 'a', sizeof too_long_value);
	too_long_value[sizeof too_long_value - 1] = '\0';
	if (valid_bignum(too_long_value))
		return 1;

	p = bignum_new(value);
	if (p == NULL)
		return 1;

	bignum_set(p, value);

	bignum_set(&a, "ff");
	bignum_set(&b, "");
	if (bignum_cmp(&a, &b) >= 0)
		die("1");

	if (bignum_cmp(&b, &a) <= 0)
		die("2");

	if (bignum_cmp(&a, &a) != 0)
		die("3");

	if (!bignum_add(&c, &a, &b))
		die("4");

	if (bignum_cmp(&c, &a) != 0)
		die("5");

	bignum_set(&a, "ff");
	bignum_set(&b, "01");
	if (!bignum_add(&c, &a, &b))
		die("Could not add\n");

	bignum_set(&facit, "0100");
	if (bignum_cmp(&c, &facit) != 0) {
		dump(&c, "c");
		dump(&facit, "Facit");
		return 1;
	}

	// We can add 0 to maxval and get maxval.
	bignum_set(&a, maxval);
	bignum_set(&b, "00");
	if (!bignum_add(&c, &a, &b))
		return 1;

	// We can not add 1 to maxval.
	bignum_set(&a, maxval);
	bignum_set(&b, "01");
	if (bignum_add(&c, &a, &b))
		return 1;
	if (bignum_add(&c, &b, &a))
		return 1;

	bignum_free(p);
	return 0;
}

#endif
