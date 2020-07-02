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
    int matches, eflags = 0;
    size_t i, maxmatches = sizeof p->matches / sizeof *p->matches;

    assert(p != NULL);
    assert(haystack != NULL);
    assert(p->compiled_ok);

    p->result = regexec(&p->re, haystack,
        maxmatches, p->matches, eflags);

    if (p->result != 0)
        return p->result == REG_NOMATCH ? 0 : -1;

    // Count matches
    matches = 0;
    for (i = 0; i < maxmatches; i++) {
        if (p->matches[i].rm_so == -1)
            break;
        matches++;
    }

    return matches;
}

size_t regex_error(regex p, char *buf, size_t bufsize)
{
    assert(p != NULL);

    return regerror(p->result, &p->re, buf, bufsize);
}

void regex_get_match(regex p, int index, size_t *pso, size_t *peo)
{
    assert(p != NULL);
    assert(index >= 0);
    assert(pso != NULL);
    assert(peo != NULL);

    assert(p->compiled_ok);
    assert((size_t)index < sizeof p->matches / sizeof *p->matches);
    assert(p->matches[index].rm_so != -1);
    assert(p->matches[index].rm_eo != -1);

    *pso = p->matches[index].rm_so;
    *peo = p->matches[index].rm_eo;
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
    } tests[] = {
        { "[a-c-e]",                    "abc",              failure, -1},    // intentional error
        { "abc",                        "abc",              success,   1},
        { "abc",                        "def",              success,   0},
        { "(abc)",                      "abc",              success,   2},
        { "foobar",                     "xxfoobarxx",       success,   1},
        { "(foo)(bar)",                 "xxfoobarxx",       success,   3},
        { "ab*",                        "abc",              success,   1},
        { "ab*",                        "abcdefghiabcde",   success,   1},
        { "ab.*",                       "xxabcdefghiabcde", success,   1},
        { "(abc|def)",                  "abc",              success,   2},
        { "abc",                        "xabc",             success,   1},
        { "abc",                        "abcx",             success,   1},
        { "abc",                        "xabcx",            success,   1},
        { "abc",                        "abc abc",          success,   1},
        { "(abc)",                      "abc abc",          success,   2},
        { "(abc){1,}",                  "abcabcabc",        success,   2},
        { "(@index\\()([^)]*)\\)",      "@index(foo)",      success,   3},
        { "@(index|xref)(\\([^)]*\\))", "@index(foo)",      success,   3},
        { "@(index|xref)\\([^\\)]*\\)", "@xref(bar)",       success,   2},
        { "@(index|xref)\\([^)]*\\)",   "@xref(baz",        success,   0},
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
    }

    regex_free(p);
    return retcode;
}

#endif
