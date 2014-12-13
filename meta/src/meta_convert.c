#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <meta_convert.h>

status_t toint(const char *src, int *dest)
{
	long res;

	assert(src != NULL);
	assert(dest != NULL);

	if (!tolong(src, &res))
		return failure;

	if (res < INT_MIN || res > INT_MAX) {
		errno = ERANGE;
		return failure;
	}

	*dest = (int)res;
	return success;
}

status_t touint(const char *src, unsigned *dest)
{
	unsigned long res;

	assert(src != NULL);
	assert(dest != NULL);

	if (!toulong(src, &res))
		return failure;

	if (res > UINT_MAX) {
		errno = ERANGE;
		return failure;
	}

	*dest = (unsigned)res;
	return success;
}

status_t tolong(const char *src, long *dest)
{
	long res;
	int old_errno;
	char *endp;

	assert(src != NULL);
	assert(dest != NULL);
	
	old_errno = errno;
	errno = 0;
	
	res = strtol(src, &endp, 10);
	if (res == LONG_MIN && errno == ERANGE)
		return failure;

	if (res == LONG_MAX && errno == ERANGE)
		return failure;

	if (res == 0 && errno == EINVAL)
		return failure;

	if (endp == src) {
		// empty string
		errno = EINVAL;
		return failure;
	}

	if (*endp != '\0') {
		errno = EINVAL; // trailing chars
		return failure;
	}

	errno = old_errno;
	*dest = res;
	return success;
}

status_t toulong(const char *src, unsigned long *dest)
{
	unsigned long res;
	int old_errno;
	char *endp;

	assert(src != NULL);
	assert(dest != NULL);
	
	old_errno = errno;
	errno = 0;
	
	res = strtoul(src, &endp, 10);
	if (res == ULONG_MAX && errno == ERANGE)
		return failure;

	if (res == 0 && errno == EINVAL)
		return failure;

	if (endp == src) {
		// empty string
		errno = EINVAL;
		return failure;
	}

	if (*endp != '\0') {
		errno = EINVAL; // trailing chars
		return failure;
	}

	errno = old_errno;
	*dest = res;
	return success;
}

status_t tofloat(const char *src, float *dest)
{
	float res;
	int old_errno;
	char *endp;

	assert(src != NULL);
	assert(dest != NULL);
	
	old_errno = errno;
	errno = 0;
	
	res = strtof(src, &endp);
	if (errno == ERANGE)
		return failure;

	if (endp == src) {
		// empty string
		errno = EINVAL;
		return failure;
	}

	if (*endp != '\0') {
		errno = EINVAL; // trailing chars
		return failure;
	}

	errno = old_errno;
	*dest = res;
	return success;
}
status_t todouble(const char *src, double *dest)
{
	double res;
	int old_errno;
	char *endp;

	assert(src != NULL);
	assert(dest != NULL);
	
	old_errno = errno;
	errno = 0;
	
	res = strtod(src, &endp);
	if (errno == ERANGE)
		return failure;

	if (endp == src) {
		// empty string
		errno = EINVAL;
		return failure;
	}

	if (*endp != '\0') {
		errno = EINVAL; // trailing chars
		return failure;
	}

	errno = old_errno;
	*dest = res;
	return success;
}

#ifdef CHECK_CONVERT

#include <stdio.h>

struct itest {
	const char *src;
	status_t expected_result;
	int expected_value;
} itests[] = {
	{ "-9999999999", failure, 0 },
	{ "9999999999", failure, 0 },
	{ "-1", success, -1 },
	{ "0", success, 0 },
	{ "1", success, 1 },
	{ "", failure, 0 },
};

struct utest {
	const char *src;
	status_t expected_result;
	unsigned expected_value;
} utests[] = {
	{ "-9999999999", failure, 0 },
	{ "9999999999", failure, 0 },
	{ "-1", failure, -1 },
	{ "0", success, 0 },
	{ "1", success, 1 },
	{ "", failure, 0 },
};

int main(void)
{
	status_t rc;
	size_t i, n;

	int ires;
	unsigned ures;
	//float f;
	//double d;
	//long l;
	//unsigned long ul;

	n = sizeof itests / sizeof *itests;
	for (i = 0; i < n; i++) {
		if (itests[i].expected_result == success) {
			// Check that isint() returns true.
			if (!isint(itests[i].src)) {
				fprintf(stderr, "%s was not interpreted as int.\n", 
					itests[i].src);
				return 1;
			}
		}

		rc = toint(itests[i].src, &ires);
		if (rc != itests[i].expected_result)  {
			fprintf(stderr, "Conversion error for test %zu, value: %s\n", i, itests[i].src);
			return 1;
		}

		if (rc == success && ires != itests[i].expected_value) {
			fprintf(stderr, "Incorrect result for buf %s: expected %d, got %d\n", 
				itests[i].src, 
				itests[i].expected_value, ires);
			return 1;
		}
	}

	n = sizeof utests / sizeof *utests;
	for (i = 0; i < n; i++) {
		rc = touint(utests[i].src, &ures);
		if (rc != utests[i].expected_result)  {
			fprintf(stderr, "Conversion error for test %zu, value: %s\n", i, utests[i].src);
			return 1;
		}

		if (rc == success && ures != utests[i].expected_value) {
			fprintf(stderr, "Incorrect result for buf %s: expected %u, got %u\n", 
				utests[i].src, 
				utests[i].expected_value, ures);
			return 1;
		}
	}


	return 0;
}

#endif



