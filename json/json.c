/* Simple JSON parser written by me. */
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "meta_common.h"
#include "meta_list.h"

#include "json.h"


// * Objects start with { and end with }
// * Arrays start with [ and end with ]
// * stuff is name : value pairs, where name is a quoted string,
//   and value is one of: quoted string, number, true, false, null, array, object
// * array entries are also value, see above list of alternatives. No name in arrays though.
// OK, so we need some generic data structure to store everything in. We store
// one of the following: string, number, array, object, true|false|null.
enum valuetype {
    VAL_UNKNOWN,
    VAL_QSTRING,
    VAL_INTEGER,
    VAL_ARRAY,
    VAL_OBJECT,
    VAL_TRUE,
    VAL_FALSE,
    VAL_NULL,
    VAL_DOUBLE,
    VAL_STRING, // Not a quoted string, but the string keyword from oas 3.0.3
    VAL_BOOLEAN, // The boolean keyword from oas 3.0.3
};

enum tokentype {
    TOK_ERROR,
    TOK_UNKNOWN,
    TOK_QSTRING,
    TOK_TRUE,
    TOK_FALSE,
    TOK_COLON,
    TOK_COMMA,
    TOK_OBJECTSTART,
    TOK_OBJECTEND,
    TOK_ARRAYSTART,
    TOK_ARRAYEND,
    TOK_NUMBER,
    TOK_NULL,
    TOK_EOF,
    TOK_STRING, // This is not a quoted string, but the string keyword from oas 3.0.3
    TOK_BOOLEAN, // This is the boolean keyword from oas 3.0.3
};


// We store our input buffer in one of these, to make it easier to
// handle offset positions when we read from functions.
struct buffer {
    const char *mem;
    size_t nread, size;

    enum tokentype token;

    // We don't want to copy values from the source to a temp
    // buffer, as we don't want to allocate memory, and we don't
    // want a fixed size buffer either. So we just point to
    // the start(first char) and end(last char) of the value we
    // found in the source memory(the mem pointer above)
    const char *value_start, *value_end;

    char *savedvalue;
    size_t valuesize;

    // We need to trace the nesting of arrays somewhere, and we
    // want to be thread safe too. So this is the place. We just
    // add 1 for '[' and subtract 1 for ']'. We barf on "[]]".
    // We may also want to barf if narrays > 0 at end of input.
    int narrays;
};

struct object {
    char *name;
    struct value *value;
};

struct value {
    // Which value type are we storing here?
    enum valuetype type;

    union {
        char *sval; // string
        long lval;  // long integers
        long double ldval; // floating point numbers
        list aval; // array value, all struct value-pointers.
        list oval; // object value.
    } v;
};

struct json_parser {
    struct value *jp_value;
    unsigned jp_errno;
    unsigned long lineno;
    struct buffer jp_buf;
};

static void value_free(struct value *p);

static void object_free(struct object *p)
{
    if (p != NULL) {
        free(p->name);
        value_free(p->value);
        free(p);
    }
}

static void value_free(struct value *p)
{
    if (p == NULL)
        return;

    if (p->type == VAL_ARRAY)
        list_free(p->v.aval, (dtor)value_free);
    else if (p->type == VAL_QSTRING)
        free(p->v.sval);
    else if (p->type == VAL_OBJECT)
        list_free(p->v.oval, (dtor)object_free);

    free(p);
}

__attribute__((malloc))
__attribute__((warn_unused_result))
static char *dupstr(const char *src)
{
    assert(src != NULL);

    size_t n = strlen(src) + 1;
    char *result = malloc(n);
    if (result != NULL)
        memcpy(result, src, n);

    return result;
}

// name and value may be NULL
__attribute__((warn_unused_result))
static struct object* object_new(const char *name, struct value *value)
{
    struct object *p;

    if ((p = calloc(1, sizeof *p)) != NULL) {
        if (name != NULL && (p->name = dupstr(name)) == NULL) {
            free(p);
            return NULL;
        }

        p->value = value;
    }

    return p;
}

__attribute__((warn_unused_result))
static inline const char* buffer_currpos(struct buffer *p)
{
    assert(p != NULL);
    return p->mem + p->nread;
}

__attribute__((warn_unused_result))
static inline int buffer_getc(struct buffer *p)
{
    if (p->nread == p->size)
        return EOF;

    return p->mem[p->nread++];
}

static inline void buffer_ungetc(struct buffer *p)
{
    assert(p != NULL);
    assert(p->nread > 0);

    p->nread--;
}

static inline const char* buffer_cstr(struct buffer *p)
{
    assert(p != NULL);
    return p->mem + p->nread;
}

// forward declarations because of recursive calls
static struct object* accept_object(struct json_parser *src);
static list accept_objects(struct json_parser *src);


// Is the value equal to "0"?
static bool is_zero(const char *s)
{
    return *s == '0' && *(s + 1) == '\0';
}

// 0123 is illegal, so is -0123. Leading zeros are illegal.
static bool has_leading_zero(const char *s)
{
    while (isspace(*s))
        s++;

    if (*s == '\0')
        return false;

    // Skip negative sign, if present
    if (*s == '-')
        s++;

    if (*s != '0')
        return false;

    // Is next character a digit? If so, we have leading zero
    return isdigit(*(s + 1));
}

// Is the value of s an integer number, as specified by
// https://www.json.org/img/number.png
// It can be "0", "-0", "[1-9][0-9]*"
// It cannot contain ., e, or anything else.
// Leading zeroes are uncool
// Update: Should we treat 10E10 as integer or real?
static bool isinteger(const char *s)
{
    assert(s != NULL);

    // Skip leading -, if any.
    if (*s == '-')
        s++;

    // From now on, it's digits only, with a special
    // test for "0". If 0 is found, then it cannot have trailing digits.
    if (is_zero(s))
        return true;

    // Leading zeros are illegal in JSON
    if (has_leading_zero(s))
        return false;

    // if first digit is 1..9 and the rest of the string is 0..9,
    // then we have a legal integer number.
    if (*s >= '1' && *s <= '9') {
        // Now the rest of the string must be digits too.
        while (*++s != '\0')
            if (!isdigit(*s))
                return false;

        // Got to the end of the string, all digits. Yay.
        return true;
    }

    // Not an integer if we got to this point
    return false;
}

// return true if last char is '.'
static bool has_trailing_dots(const char *s)
{
    assert(s != NULL);

    size_t len = strlen(s);
    if (len == 0)
        return false;

    return s[len - 1] == '.';
}


// Is the value in s a real number, with fractions(.n) and/or an exponent?
// It's not a real if it's an integer. Imma be lazy here and use
// strtod to test if it's a real number. If strtod() consumes all chars,
// it a real number.
// Note that leading zeroes are uncool here too. We may want to
// merge parts of this function with parts of isinteger().
//
// update 2021013: JSONTestSuite is stricter than strtod(), so we must
// do more testing ourselves. Here's one example of an illegal number:
//      [-2.]
// Trailing dots are not acceptable. We must detect them ourselves.
//
static bool isreal(const char *s)
{
    assert(s != NULL);
    assert(*s != '\0'); // Empty strings? Nah, meaningless

    // Remove leading ws
    while (isspace(*s))
        s++;

    // Was the string all ws? If so, it's not a real number.
    if (*s == '\0')
        return false;

    if (isinteger(s))
        return false;

    // From now on, it's digits only, with a special
    // test for "0". If 0 is found, then it cannot have trailing digits.
    if (is_zero(s))
        return true;

    // Leading zeros are illegal in JSON
    if (has_leading_zero(s))
        return false;

    // Trailing dots, "2." are illegal in JSON
    if (has_trailing_dots(s))
        return false;

    // A fraction is required if we have a dot. "0.e1" is illegal in JSON
    const char *p = strchr(s, '.');
    if (p != NULL && !isdigit(*(p + 1)))
        return false;

    // If we have a fraction, we also need an integer part. ".1" is illegal
    if (p != NULL) {
        if (p == s)
            return false;

        if (!isdigit(*(p - 1)))
            return false;
    }

    bool result;
    char *endp;
    int olderrno = errno;
    errno = 0;
    size_t len = strlen(s);


    long double val = strtold(s, &endp);
    if (errno != 0)
        result = false;
    else if (endp != s + len) // We didn't consume all chars
        result = false;
    else
        result = true;

    (void)val;
    errno = olderrno;
    return result;
}

static bool isnumber(const char *s, size_t nchars)
{
    char str[512];

    if (nchars >= sizeof str)
        return false;

    memcpy(str, s, nchars);
    str[nchars] = '\0';

    // printf("src:%s\n", str);

    return isinteger(str) || isreal(str);
}

// some member functions
static struct value * value_new(enum valuetype type, void *value)
{
    struct value *p;

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    p->type = type;
    switch (type) {
        case VAL_QSTRING:
            if ((p->v.sval = dupstr(value)) == NULL)
                goto memerr;
            break;

        case VAL_DOUBLE:
            assert(isreal(value));
            p->v.ldval = strtold(value, NULL);
            break;

        case VAL_INTEGER:
            assert(isinteger(value));
            p->v.lval = strtol(value, NULL, 10);
            break;

        case VAL_ARRAY:
            p->v.aval = value;
            break;

        case VAL_OBJECT:
            p->v.oval = value;
            break;

        case VAL_UNKNOWN:
            die("Can't deal with unknown types");

        case VAL_BOOLEAN:
        case VAL_STRING:
        case VAL_TRUE:
        case VAL_FALSE:
        case VAL_NULL:
            // handled directly via type
            break;

        default:
            die("Dude, wtf? Handle all values, please\n");
    }

    return p;

memerr:
    free(p);
    return NULL;
}

static const struct {
    bool hasvalue;
    const char *text;
} tokens[] = {
    [ TOK_ERROR       ] = { false, "error" },
    [ TOK_UNKNOWN     ] = { false, "unknown" },
    [ TOK_QSTRING     ] = { true,  "qstring" },
    [ TOK_TRUE        ] = { false, "true" },
    [ TOK_FALSE       ] = { false, "false" },
    [ TOK_COLON       ] = { false, "colon" },
    [ TOK_COMMA       ] = { false, "comma" },
    [ TOK_OBJECTSTART ] = { false, "objectstart" },
    [ TOK_OBJECTEND   ] = { false, "objectend" },
    [ TOK_ARRAYSTART  ] = { false, "arraystart" },
    [ TOK_ARRAYEND    ] = { false, "arrayend" },
    [ TOK_NUMBER      ] = { true,  "number" },
    [ TOK_NULL        ] = { false, "null" },
    [ TOK_EOF         ] = { false, "eof" },
    [ TOK_STRING      ] = { false, "string" },
};

static inline bool four_hex_digits(struct buffer *src)
{
    assert(src != NULL);

    const char *s = buffer_cstr(src);
    size_t len = strlen(s);

    // we need four characters
    if (len < 4)
        return false;

    // they must all be hex digits
    return isxdigit(s[0]) && isxdigit(s[1]) && isxdigit(s[2]) & isxdigit(s[3]);
}


static inline status_t retfail(struct json_parser *p, int err)
{
    assert(p != NULL);
    p->jp_errno = err;
    return failure;
}

// We have a ", which is start of quoted string. Read the rest of the
// string and place it in value. Remember that escapes, \, may be
// escaped too, like \\. If so, read them as one backslash so "\\" results
// in a string with just one backslash.
//
// There seems to be some rules in JSON regarding illegal characters.
// * TAB is uncool, see fail25.json and fail26.json.
// * Only some chars can be escaped:
//      "\/bfnrt as well as u, followed by four hex digits
//
// Beware of quoted strings like "\"" (one quote is all we got). In that case,
// the \" has already been consumed and we will have zero data here. Detect
// that corner case.
static int get_qstring(struct json_parser *p)
{
    const char *legal_escapes = "\\\"/bfnrtu";
    int c, prev = 0;
    size_t nread = 0;

    assert(p != NULL);
    p->jp_buf.value_start = p->jp_buf.value_end = buffer_currpos(&p->jp_buf);

    while ((c = buffer_getc(&p->jp_buf)) != EOF) {
        nread++;

        // If escape char is u, four hex digits MUST follow
        if (prev == '\\' && (c == 'u' || c == 'U')) {
            if (!four_hex_digits(&p->jp_buf))
                return TOK_UNKNOWN;
        }

        // Don't escape the NIL character
        if (prev == '\\' && c == '\0')
            return TOK_UNKNOWN;

        // Don't even pass the NIL character, regardless of escapes
        if (c == '\0')
            return TOK_UNKNOWN;

        // Check for legal escape sequence
        if (prev == '\\' && strchr(legal_escapes, c) == NULL)
            return TOK_UNKNOWN;

        if (c == '\\' && prev == '\\')
            prev = 0;
        else if (c == '"' && prev != '\\')
            break;
        else
            prev = c;


        // Can't have these in strings
        if (c == '\t' || c == '\n')
            return TOK_UNKNOWN;

        p->jp_buf.value_end++;
    }

    if (nread == 0)
        return TOK_UNKNOWN;

    // The string must end with a quote
    if (c != '"')
        return TOK_UNKNOWN;

    return TOK_QSTRING;
}

// We've read a t. Are the next characters rue, as in true?
static int get_true(struct buffer *src)
{
    assert(src != NULL);

    if (buffer_getc(src) == 'r'
    && buffer_getc(src) == 'u'
    && buffer_getc(src) == 'e')
        return TOK_TRUE;

    return TOK_UNKNOWN;
}

static int get_false(struct buffer *src)
{
    assert(src != NULL);

    if (buffer_getc(src) == 'a'
    && buffer_getc(src) == 'l'
    && buffer_getc(src) == 's'
    && buffer_getc(src) == 'e')
        return TOK_FALSE;

    return TOK_UNKNOWN;
}

static int get_null(struct buffer *src)
{
    assert(src != NULL);

    if (buffer_getc(src) == 'u'
    && buffer_getc(src) == 'l'
    && buffer_getc(src) == 'l')
        return TOK_NULL;

    return TOK_UNKNOWN;
}

static int get_string(struct buffer *src)
{
    assert(src != NULL);

    if (buffer_getc(src) == 't'
    && buffer_getc(src) == 'r'
    && buffer_getc(src) == 'i'
    && buffer_getc(src) == 'n'
    && buffer_getc(src) == 'g')
        return TOK_STRING;

    return TOK_UNKNOWN;
}

static int get_boolean(struct buffer *src)
{
    assert(src != NULL);

    if (buffer_getc(src) == 'o'
    && buffer_getc(src) == 'o'
    && buffer_getc(src) == 'l'
    && buffer_getc(src) == 'e'
    && buffer_getc(src) == 'a'
    && buffer_getc(src) == 'n')
        return TOK_BOOLEAN;

    return TOK_UNKNOWN;
}

// Read digits and place them in buffer, ungetc() the first non-digit
// so we can read it the next time.
//
// The value may be an integer or a real number, with or without
// an exponent part. The BNF just mentions numbers. Do we really
// want to distinguish between integers and doubles at this point?
// I guess not. We can't fucking introduce new tokens here, right? What
// was I thinking?
static status_t get_number(struct json_parser *parser)
{
    int c;
    const char *legal = "0123456789-.eE+";
    size_t nchars = 0;

    assert(parser != NULL);

    struct buffer *src = &parser->jp_buf;
    assert(src != NULL);

    src->value_start = src->value_end = buffer_currpos(src);

    while ((c = buffer_getc(src)) != EOF) {
        if (c == '\0') {
            src->token = TOK_UNKNOWN;
            return retfail(parser, EINVAL);
        }

        if (strchr(legal, c)) {
            src->value_end++;
            nchars++;
        }
        else {
            buffer_ungetc(src);
            break;
        }
    }

    // Now we have a sequence of numbers and characters. Do they
    // constitue a legal number? "+++---123eeee123" is not a legal
    // number, so we must check. ATM we don't have a string, only a buffer,
    // so some extra consideration is needed.
    if (!isnumber(src->value_start, nchars)) {
        src->token = TOK_UNKNOWN;
        return retfail(parser, EINVAL);
    }

    src->token = TOK_NUMBER;
    return success;
}

static inline bool hasvalue(enum tokentype tok)
{
    assert(tok < sizeof tokens / sizeof *tokens);
    return tokens[tok].hasvalue;
}

__attribute__((warn_unused_result))
static status_t nextsym(struct json_parser *p)
{
    int c;

    if (p->jp_buf.token == TOK_EOF)
        return retfail(p, ENOENT);

    p->jp_buf.value_start =p->jp_buf.value_end = NULL;
    p->jp_buf.token = TOK_UNKNOWN;

    // skip ws
    while ((c = buffer_getc(&p->jp_buf)) != EOF && isspace(c)) {
        // Form feeds aren't legal. Applies to many other control chars too?
        // TODO: But form feeds can be escaped. Fix this.
        if (c == '\f')
            return retfail(p, EINVAL);

        if (c == '\n')
            p->lineno++;
    }

    switch (c) {
        case EOF: p->jp_buf.token = TOK_EOF; break;
        case '[': p->jp_buf.token = TOK_ARRAYSTART; break;
        case ']': p->jp_buf.token = TOK_ARRAYEND; break;
        case '{': p->jp_buf.token = TOK_OBJECTSTART; break;
        case '}': p->jp_buf.token = TOK_OBJECTEND; break;
        case ':': p->jp_buf.token = TOK_COLON; break;
        case ',': p->jp_buf.token = TOK_COMMA; break;

        case '"': p->jp_buf.token = get_qstring(p); break;
        case 't': p->jp_buf.token = get_true(&p->jp_buf); break;
        case 'f': p->jp_buf.token = get_false(&p->jp_buf); break;
        case 'n': p->jp_buf.token = get_null(&p->jp_buf); break;
        case 's': p->jp_buf.token = get_string(&p->jp_buf); break;
        case 'b': p->jp_buf.token = get_boolean(&p->jp_buf); break;

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            buffer_ungetc(&p->jp_buf);
            return get_number(p);

        default:
#if 0
            fprintf(stderr, "%s(), around line %lu: unexpected token: char's int value == %d.",
                __func__, p->lineno, c);
            if (isprint(c))
                fprintf(stderr, " char value: '%c'", c);
            fprintf(stderr, "\n");
#endif
            p->jp_buf.token = TOK_UNKNOWN;
            break;
    }

    // Did we actually get a legal token?
    return p->jp_buf.token != TOK_UNKNOWN ? success : failure;
}

__attribute__((warn_unused_result))
static status_t savevalue(struct json_parser *parser)
{
    assert(parser != NULL);

    struct buffer *p = &parser->jp_buf;

    assert(p != NULL);
    assert(p->value_start != NULL);
    assert(p->value_end != NULL);
    assert(p->value_end >= p->value_start); // At least one char makes sense
    assert(p->savedvalue != NULL);
    assert(p->valuesize != 0);

    // How many bytes do we need?
    size_t n = p->value_end - p->value_start + 1;

    // reallocate on demand
    if (n > p->valuesize) {
        char *tmp = realloc(p->savedvalue, n);
        if (tmp == NULL)
            return retfail(parser, errno);

        p->savedvalue = tmp;
        p->valuesize = n;
    }

    memcpy(p->savedvalue, p->value_start, n - 1);
    p->savedvalue[n - 1] = '\0'; // terminate the new string
    return success;
}

__attribute__((warn_unused_result))
static status_t accept(struct json_parser *p, enum tokentype tok)
{
    if (p->jp_buf.token == tok) {
        if (hasvalue(tok) && !savevalue(p))
            return retfail(p, EINVAL);

        if (nextsym(p))
            return success;
    }

    return retfail(p, EINVAL);
}

static struct value* accept_value(struct json_parser *p)
    __attribute__((warn_unused_result));

// Notes on fail18.json:
// * Issue is when arrays nest too deep. Not sure why json has this
// limitation. (Maybe to avoid exploits trying to DOS the parser?)
// Anyway, we impose some limit. Test data indicates that 19 is max.
#define MAX_ARRAY_NESTING 19

static struct value *accept_array_elements(struct json_parser *p)
{
    list lst = NULL;
    struct value *value;
    int ncommas = -1, nvalues = 0;

    // fail18: Can't nest too deep.
    p->jp_buf.narrays++;
    if (p->jp_buf.narrays > MAX_ARRAY_NESTING) {
        list_free(lst, (dtor)value_free);
        return NULL;
    }

    do {
        value = accept_value(p);

        // corner case: First element in array is missing. See fail6.json
        if (lst == NULL && value == NULL)
            break;

        if (value != NULL) {
            lst = list_add(lst, value);
            if (lst == NULL)
                return NULL;

            nvalues++;
        }
        ncommas++;
    } while (accept(p, TOK_COMMA));

    // Note that we do need to be at token ARRAY END
    if (!accept(p, TOK_ARRAYEND)) {
        list_free(lst, (dtor)value_free);
        return NULL;
    }
    p->jp_buf.narrays--;

    // handle fail4.json: The number of commas should equal
    // the number of values minus one.
    if (nvalues > 0 && nvalues - ncommas != 1) {
        list_free(lst, (dtor)value_free);
        return NULL;
    }

    // Wrap array list in a struct value object before returning
    return value_new(VAL_ARRAY, lst);
}

__attribute__((warn_unused_result))
static struct value* accept_value(struct json_parser *p)
{
    assert(p != NULL);

    if (accept(p, TOK_OBJECTSTART)) {
        list lst = accept_objects(p);
        if (lst == NULL)
            return NULL;

        return value_new(VAL_OBJECT, lst);
    }

    if (accept(p, TOK_ARRAYSTART))
        return accept_array_elements(p);

    if (accept(p, TOK_TRUE))
        return value_new(VAL_TRUE, NULL);

    if (accept(p, TOK_FALSE))
        return value_new(VAL_FALSE, NULL);

    if (accept(p, TOK_NULL))
        return value_new(VAL_NULL, NULL);

    if (accept(p, TOK_NUMBER)) {
        if (isinteger(p->jp_buf.savedvalue))
            return value_new(VAL_INTEGER, p->jp_buf.savedvalue);

        if (isreal(p->jp_buf.savedvalue))
            return value_new(VAL_DOUBLE, p->jp_buf.savedvalue);

        die("Internal error. Unhandled number from tokenizer");
    }

    if (accept(p, TOK_QSTRING))
        return value_new(VAL_QSTRING, p->jp_buf.savedvalue);

    if (accept(p, TOK_STRING))
        return value_new(VAL_STRING, NULL);

    if (accept(p, TOK_BOOLEAN))
        return value_new(VAL_BOOLEAN, NULL);

    return NULL;
}


// Accept one object, i.e., a name:value pair.
// Opening brace HAS been read already
// Value may be null, as in { "foo" : { } }
__attribute__((warn_unused_result))
static struct object* accept_object(struct json_parser *p)
{
    // Look for a name
    if (!accept(p, TOK_QSTRING))
        return NULL;

    // Name is now in the buffer's saved value. Create an object
    struct object *obj = object_new(p->jp_buf.savedvalue, NULL);
    if (obj == NULL)
        return NULL;

    // We need the : which separates name and value
    if (!accept(p, TOK_COLON))
        goto error;

    // Now get the value
    obj->value = accept_value(p);
    if (obj->value != NULL)
        return obj;

error:
    object_free(obj);
    return NULL;
}

// Read objects, which are name:value pairs separated by commas.
// The opening { has been read already. We read the closing }.
__attribute__((warn_unused_result))
static list accept_objects(struct json_parser *src)
{
    list result;
    struct object *obj = NULL;
    int ncommas = -1, nobjects = 0;

    if ((result = list_new()) == NULL)
        return NULL;

    do {
        if ((obj = accept_object(src)) != NULL) {
            if (list_add(result, obj) == NULL)
                goto enomem;

            nobjects++;
        }

        ncommas++;
    } while (accept(src, TOK_COMMA));

    if (ncommas > 0 && nobjects - ncommas != 1) {
        // too many commas.
        list_free(result, (dtor)object_free);
        return NULL;
    }

    if (!accept(src, TOK_OBJECTEND)) {
        list_free(result, (dtor)object_free);
        return NULL;
    }

    return result;

enomem:
    list_free(result, (dtor)object_free);
    object_free(obj);
    return NULL;
}

__attribute__((warn_unused_result))
static status_t buffer_init(struct json_parser *parser, const void *src, size_t srclen)
{
    assert(parser != NULL);
    assert(src != NULL);
    assert(srclen > 0);

    struct buffer *p = &parser->jp_buf;
    memset(p, 0, sizeof *p);
    p->mem = src;
    p->size = srclen;
    p->narrays = 0;

    // pre-allocate the value buffer to avoid too many malloc/free cycles
    p->valuesize = 4096;
    if ((p->savedvalue = malloc(p->valuesize)) == NULL)
        return retfail(parser, errno);

    return success;
}

static void buffer_cleanup(struct buffer *p)
{
    assert(p != NULL);

    free(p->savedvalue);
    p->savedvalue = NULL;
    p->valuesize = 0;
}

status_t json_parse(struct json_parser *p)
{
    assert(p != NULL);

    // Load first symbol
    if (!nextsym(p))
        return retfail(p, ENOENT);

    p->jp_value = accept_value(p);
    if (p->jp_value == NULL)
        return retfail(p, EINVAL);

    // Something's fucked if we still have tokens. This is syntax error 
    // in input, not our error. We just need to deal with it.
    if (nextsym(p))
        return retfail(p, E2BIG);

    return success;
}

struct json_parser *json_parser_new(const void *src, size_t srclen)
{
    struct json_parser *p;

    assert(src != NULL);
    assert(srclen > 0);

    p = calloc(1, sizeof *p);
    if (p == NULL)
        return NULL;

    p->lineno = 1;
    if (!buffer_init(p, src, srclen)) {
        free(p);
        return NULL;
    }

    return p;
}

void json_parser_free(struct json_parser *p)
{
    if (p != NULL) {
        value_free(p->jp_value);
        buffer_cleanup(&p->jp_buf);
        free(p);
    }
}

struct value* json_parser_values(struct json_parser *p)
{
    assert(p != NULL);
    return p->jp_value;
}

int json_parser_errno(const struct json_parser *p)
{
    assert(p != NULL);
    return p->jp_errno;
}

const char* json_parser_errtext(const struct json_parser *p)
{
    static const char *text[] = {
        [0] = "no error",
    };

    assert(p != NULL);
    if ((size_t)p->jp_errno < sizeof text / sizeof *text)
        return text[p->jp_errno];

    return "unknown error";
}

#ifdef JSON_CHECK

static void print_value(struct value *p);
static void print_array(list lst)
{
    list_iterator i;

    if (lst == NULL) {
        printf("[ ]\n");
        return;
    }

    printf("[\n");

    for (i = list_first(lst); !list_end(i); i = list_next(i)) {
        struct value *v = list_get(i);
        if (v == NULL)
            printf("(no val in lst)");
        else
            print_value(v);

        if (!list_last(i))
            printf(",\n");
    }

    printf("]\n");
}

static void print_object(const struct object *p)
{
    assert(p != NULL);

    if (p->name != NULL) {
        printf("\"%s\" :", p->name);
        print_value(p->value);
    }
}


// object lists can be NULL, in case input is "{}"
static void print_objects(list lst)
{
    list_iterator i;

    printf("{\n");

    if (lst == NULL)
        goto end;

    for (i = list_first(lst); !list_end(i); i = list_next(i)) {
        struct object *obj = list_get(i);
        if (obj == NULL)
            printf("(no object in lst)");
        else
            print_object(obj);

        if (!list_last(i))
            printf(",\n");
    }

end:
    printf("\n}");
}

static void print_value(struct value *p)
{
    assert(p != NULL);

    switch (p->type) {
        case VAL_UNKNOWN:
            printf("unknown");
            break;

        case VAL_QSTRING:
            printf("\"%s\"", p->v.sval);
            break;

        case VAL_INTEGER:
            printf("%ld", p->v.lval);
            break;

        case VAL_ARRAY:
            print_array(p->v.aval);
            break;

        case VAL_OBJECT:
            print_objects(p->v.oval);
            break;

        case VAL_STRING:
            printf("string");
            break;

        case VAL_TRUE:
            printf("true");
            break;

        case VAL_FALSE:
            printf("false");
            break;

        case VAL_BOOLEAN:
            printf("boolean");
            break;

        case VAL_NULL:
            printf("null");
            break;

        case VAL_DOUBLE:
            printf("%Lg", p->v.ldval);
            break;
    }
}

void json_free(struct value *objects)
{
    value_free(objects);
}

void json_traverse(struct value *value)
{
    print_value(value);
}


static status_t testfile(const char *filename)
{
    int fd = -1;
    struct stat st;


    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return failure;

    if (fstat(fd, &st)) {
        close(fd);
        return failure;
    }

    if (st.st_size < 1) {
        // File too small to bother with
        close(fd);
        errno = EINVAL;
        return failure;
    }

    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (mem == MAP_FAILED)
        return failure;

    struct json_parser *parser = json_parser_new(mem, st.st_size);
    if (parser == NULL)
        return failure;

    status_t status = json_parse(parser);
    munmap(mem, st.st_size);

    // Print error message, if any.
    if (status == failure) {
        int err = json_parser_errno(parser);
        char text[1024] = {0};

        int res = strerror_r(err, text, sizeof text);
        if (res != 0)
            strcpy(text, "unknown error");

        fprintf(stderr, "%s(%ld):%s\n", filename, parser->lineno, text);
    }

    json_parser_free(parser);

    return status;
}

static void test_isinteger(void)
{
    static const struct {
        const char *value;
        bool result;
    } tests[] = {
        { "0",    true  },
        { "-0",   true  },
        { "-01",  false },
        { "-10",  true  },
        { "1",    true  },
        { "0123", false },
        { "1000", true  },
        { "1X",   false },
        { "1.2",  false },
        { "-0.5", false },
    };

    size_t i, n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        bool result = isinteger(tests[i].value);
        if (result != tests[i].result)
            die("isinteger() failed on %s.\n", tests[i].value);
    }
}

static void test_isreal(void)
{
    static const struct {
        const char *value;
        bool result;
    } tests[] = {
        { "1.2",     true },
        { "1.2e2",   true },
        { "1.2E2",   true },
        { "-0.5",    true },
        { "-0.5",    true },
        { "1000E10", true },
    };

    size_t i, n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        bool result = isreal(tests[i].value);
        if (result != tests[i].result)
            die("isreal() failed on %s.\n", tests[i].value);
    }
}

static inline const char *maptoken(enum tokentype tok)
{
    assert(tok < sizeof tokens / sizeof *tokens);
    return tokens[tok].text;
}

static void test_get_qstring(void)
{
    static const struct {
        const char *src, *str;
        int result;
    } tests[] = {
        { "hello\"", "hello", TOK_QSTRING }
    };

    size_t i, n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        struct json_parser *parser = json_parser_new(tests[i].src, strlen(tests[i].src));
        if (parser == NULL)
            die("Unable to init parser\n");

        int result = get_qstring(parser);
        if (result != tests[i].result)
            die("%s(): src: <%s>: expected %s, got %s\n",
                __func__, tests[i].src, maptoken(tests[i].result),
                maptoken(result));

        ptrdiff_t blen = parser->jp_buf.value_end - parser->jp_buf.value_start;
        size_t reslen = strlen(tests[i].str);
        if ((size_t)blen != reslen)
            die("%s(): expected %lu bytes, got %ld\n",
                __func__, reslen, blen);

        int d = memcmp(tests[i].str, parser->jp_buf.value_start, reslen);
        if (d != 0)
            die("%s(): Something's really odd\n", __func__);

        json_parser_free(parser);
    }
}

static void test_internal_functions(void)
{
    test_isinteger();
    test_isreal();
    test_get_qstring();
}

int main(int argc, char *argv[])
{
    int exitcode = 0;

    const char *filenames[] = {
        "./array_at_start.json",
        "./array_with_no_entries.json",
        "./array_with_one_entry.json",
        "./schema.json",
        "./github/ghes-3.0.json"
    };

    test_internal_functions();

    if (argc > 1) {
        while (*++argv != NULL) {
            if (0) fprintf(stderr, "Testing contents of file %s\n", *argv);
            if (!testfile(*argv))
                exitcode = 1;
        }
    }
    else {
        size_t i, n;

        n = sizeof filenames / sizeof *filenames;
        for (i = 0; i < n; i++) {
            if (0) fprintf(stderr, "Testing contents of file %s\n", filenames[i]);
            if (!testfile(filenames[i]))
                exitcode = 1;

            if (0) fprintf(stderr, "\n");
        }
    }

    return exitcode;
}

#endif
