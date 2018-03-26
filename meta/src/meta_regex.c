#include <assert.h>
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include <meta_common.h>
#include <meta_regex.h>

struct regex_tag {
    regex_t re;
    regmatch_t matches[64];

    // Save result so we can check for errors
    int result;

    // We must compile before we search.
    int compiled_ok;
};

regex regex_new(void)
{
    regex p;

    if ((p = malloc(sizeof *p)) != NULL) {
        memset(p, 0, sizeof *p);
    }

    return p;
}

void regex_free(regex p)
{
    if (p != NULL)
        free(p);
}

status_t regex_comp(regex p, const char *expr)
{
    int cflags = REG_EXTENDED;

    assert(p != NULL);
    assert(expr != NULL);

    p->compiled_ok = 0;
    p->result = regcomp(&p->re, expr, cflags);
    if (p->result == 0)
        p->compiled_ok = 1;


    return p->result == 0 ? success : failure;
}

size_t regex_error(regex p, char *buf, size_t bufsize)
{
    assert(p != NULL);

    return regerror(p->result, &p->re, buf, bufsize);
}

#ifdef CHECK_REGEX

#include <stdio.h>

int main(void)
{
    regex p;
    status_t result;
    size_t size;
    char errbuf[1000];
    const char *Success = "Success";

    p = regex_new();
    assert(p != NULL);

    result = regex_comp(p, "abc");
    assert(result == success);

    size = regex_error(p, NULL, 0);
    assert(size == strlen(Success) + 1);

    regex_error(p, errbuf, sizeof errbuf);
    assert(strcmp(errbuf, Success) == 0);

    // Can we trigger an error?
    result = regex_comp(p, "[a-c-e]");
    assert(result == failure);

    regex_free(p);
}

#endif
