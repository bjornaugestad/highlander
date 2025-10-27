#include <assert.h>
#include <errno.h>
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

    if ((p = malloc(sizeof *p)) != NULL)
        memset(p, 0, sizeof *p);

    return p;
}

void regex_free(regex p)
{
    if (p == NULL)
        return;

    if (p->compiled_ok)
        regfree(&p->re);

    free(p);
}

status_t regex_comp(regex p, const char *expr)
{
    int cflags = REG_EXTENDED;

    assert(p != NULL);
    assert(expr != NULL);

    if (p->compiled_ok)
        regfree(&p->re);

    p->compiled_ok = 0;
    p->result = regcomp(&p->re, expr, cflags);
    if (p->result == 0) {
        p->compiled_ok = 1;
        return success;
    }

    return failure;
}

int regex_exec(regex p, const char *haystack)
{
    int result, eflags = 0;
    size_t i, maxmatches = sizeof p->matches / sizeof *p->matches;

    assert(p != NULL);
    assert(haystack != NULL);
    assert(p->compiled_ok);

    p->result = regexec(&p->re, haystack, maxmatches, p->matches, eflags);
    if (p->result != 0)
        return p->result == REG_NOMATCH ? 0 : -1;

    // Count matches
    result = 0;
    for (i = 0; i < maxmatches; i++) {
        if (p->matches[i].rm_so == -1)
            break;
        result++;
    }

    return result;
}

size_t regex_error(regex p, char *buf, size_t bufsize)
{
    assert(p != NULL);

    return regerror(p->result, &p->re, buf, bufsize);
}

void regex_get_match_index(regex p, int index, size_t *pso, size_t *peo)
{
    assert(p != NULL);
    assert(index >= 0);
    assert(pso != NULL);
    assert(peo != NULL);

    assert(p->compiled_ok);
    assert((size_t)index < sizeof p->matches / sizeof *p->matches);
    assert(p->matches[index].rm_so != -1);
    assert(p->matches[index].rm_eo != -1);

    *pso = (size_t)p->matches[index].rm_so;
    *peo = (size_t)p->matches[index].rm_eo;
}

status_t regex_get_match(regex p, int index, const char *src, char *dest, size_t destsize)
{
    size_t so, eo;

    assert(p != NULL);
    assert(index >= 0);
    assert(src != NULL);
    assert(dest != NULL);
    assert(destsize > 0);

    regex_get_match_index(p, index, &so, &eo);
    if (eo - so > destsize)
        return fail(ENOSPC);

    memcpy(dest, &src[so], eo - so);
    dest[eo - so] = '\0';
    return success;
}

#ifdef CHECK_REGEX

#include <stdio.h>

int main(void)
{
    regex p;
    char errbuf[1000];

    static const struct {
        const char *re;
        const char *haystack;

        // We expect a compilation result (most times 0), and an
        // execution result. The latter varies with the expression.
        status_t compres;
        int expected;
        const char *matches[10];
    } tests[] = {
        { "[a-c-e]",                    "abc",              failure,  -1, { NULL } },    // intentional error
        { "abc",                        "abc",              success,   1, { "abc", NULL } },
        { "abc",                        "def",              success,   0, { NULL } },
        { "(abc)",                      "abc",              success,   2, { NULL } },
        { "foobar",                     "xxfoobarxx",       success,   1, {"foobar",  NULL } },
        { "(foo)(bar)",                 "xxfoobarxx",       success,   3, { "foobar", "foo", "bar", NULL } },
        { "ab*",                        "abc",              success,   1, { "ab", NULL } },
        { "ab*",                        "abcdefghiabcde",   success,   1, { "ab", NULL } },
        { "ab.*",                       "xxabcdefghiabcde", success,   1, { "abcdefghiabcde", NULL } },
        { "(abc|def)",                  "abc",              success,   2, { "abc", "abc", NULL } },
        { "abc",                        "xabc",             success,   1, { "abc", NULL } },
        { "abc",                        "abcx",             success,   1, { "abc", NULL } },
        { "abc",                        "xabcx",            success,   1, { "abc", NULL } },
        { "abc",                        "abc abc",          success,   1, { "abc", NULL } },
        { "(abc)",                      "abc abc",          success,   2, { "abc", "abc", NULL } },
        { "(abc){1,}",                  "abcabcabc",        success,   2, { "abcabcabc", "abc", NULL } },
        { "(@index\\()([^)]*)\\)",      "@index(foo)",      success,   3, { "@index(foo)", "@index(", "foo", NULL } },
        { "@(index|xref)(\\([^)]*\\))", "@index(foo)",      success,   3, { "@index(foo)", "index", "(foo)", NULL } },
        { "@(index|xref)\\([^\\)]*\\)", "@xref(bar)",       success,   2, { "@xref(bar)", "xref" } },
        { "@(index|xref)\\([^)]*\\)",   "@xref(baz",        success,   0, { NULL } },
    };
    size_t i, n;
    int retcode = 0;

    p = regex_new();
    assert(p != NULL);

    n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        int nfound;

        if (regex_comp(p, tests[i].re) != tests[i].compres) {
            regex_error(p, errbuf, sizeof errbuf);
            fprintf(stderr, "Failed to compile %s. Error: %s\n",
                tests[i].re, errbuf);
            retcode = 77;
            continue;
        }

        // Don't exec if we wanted compilation to fail
        if (tests[i].compres == failure)
            continue;

        nfound = regex_exec(p, tests[i].haystack);
        if (nfound != tests[i].expected) {
            fprintf(stderr, "Searched for \"%s\" in \"%s\". Got %d matches. Expected %d\n",
                tests[i].re, tests[i].haystack, nfound, tests[i].expected);
            retcode = 77;
            continue;
        }

        for (int j = 0; j < nfound; j++) {
            size_t so, eo;
            char buf[1024];
            char buf2[1024];
            status_t result;

            memset(buf, 0, sizeof buf);
            regex_get_match_index(p, j, &so, &eo);
            if (so == (size_t)-1 || eo == (size_t)-1) {
                fprintf(stderr, "Failed to retrieve result for test %zu, elem %d\n", i, j);
                retcode = 77;
            }

            memcpy(buf, &tests[i].haystack[so], eo - so);
            if (tests[i].matches[j] != NULL && strcmp(tests[i].matches[j], buf) != 0) {
                fprintf(stderr, "test %zu: buf(%d): expected %s, got %s\n",
                    i, j, tests[i].matches[j], buf);

                retcode = 77;
            }

            result = regex_get_match(p, j, tests[i].haystack, buf2, sizeof buf2);
            if (result == failure)
                retcode = 77;
            else if (strcmp(buf, buf2) != 0) {
                fprintf(stderr, "Expected %s, got %s\n", buf, buf2);
                retcode = 77;
            }
        }
    }

    regex_free(p);
    return retcode;
}

#endif
