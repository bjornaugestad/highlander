/* Simple JSON parser written by me. */
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
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

    unsigned long lineno;

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
        double dval; // floating point numbers
        list aval; // array value, all struct value-pointers.
        list oval; // object value. 
    } v;
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

// forward declarations because of recursive calls
static struct object* accept_object(struct buffer *src);
static list accept_objects(struct buffer *src);


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
    if (*s == '0') {
        if (*(s + 1) == '\0')
            return true;

        // If we get here, then we have a zero followed by something.
        // It's not a legal integer. Is it a legal number? Do we care?
        return false;
    }

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


// Is the value in s a real number, with fractions(.n) and/or an exponent?
// It's not a real if it's an integer. Imma be lazy here and use 
// strtod to test if it's a real number. If strtod() consumes all chars,
// it a real number. 
static bool isreal(const char *s)
{
    assert(s != NULL);
    assert(*s != '\0'); // Empty strings? Nah, meaningless

    if (isinteger(s))
        return false;

    bool result;
    char *endp;
    int olderrno = errno;
    errno = 0;
    size_t len = strlen(s);


    double val = strtod(s, &endp);
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
            p->v.dval = strtod(value, NULL);
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
static int get_qstring(struct buffer *src)
{
    const char *legal_escapes = "\\\"/bfnrtu";
    int c, prev = 0;

    assert(src != NULL);
    src->value_start = src->value_end = buffer_currpos(src);

    while ((c = buffer_getc(src)) != EOF) {
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

        src->value_end++;
    }

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
static bool get_number(struct buffer *src)
{
    int c;
    const char *legal = "0123456789-.eE+";
    size_t nchars = 0;

    assert(src != NULL);
    src->value_start = src->value_end = buffer_currpos(src);

    while ((c = buffer_getc(src)) != EOF) {
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
        return false;
    }

    src->token = TOK_NUMBER;
    return true;
}

static inline bool hasvalue(enum tokentype tok)
{
    assert(tok < sizeof tokens / sizeof *tokens);
    return tokens[tok].hasvalue;
}

__attribute__((warn_unused_result))
static bool nextsym(struct buffer *src)
{
    int c;

    if (src->token == TOK_EOF)
        return false;

    src->value_start =src->value_end = NULL;
    src->token = TOK_UNKNOWN;

    // skip ws
    while ((c = buffer_getc(src)) != EOF && isspace(c)) {
        if (c == '\n')
            src->lineno++;
    }

    switch (c) {
        case EOF: src->token = TOK_EOF; break;
        case '[': src->token = TOK_ARRAYSTART; break;
        case ']': src->token = TOK_ARRAYEND; break;
        case '{': src->token = TOK_OBJECTSTART; break;
        case '}': src->token = TOK_OBJECTEND; break;
        case ':': src->token = TOK_COLON; break;
        case ',': src->token = TOK_COMMA; break;

        case '"': src->token = get_qstring(src); break;
        case 't': src->token = get_true(src); break;
        case 'f': src->token = get_false(src); break;
        case 'n': src->token = get_null(src); break;
        case 's': src->token = get_string(src); break;
        case 'b': src->token = get_boolean(src); break;

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
            buffer_ungetc(src);
            return get_number(src);

        default:
            // fprintf(stderr, "%s(), around line %lu: unexpected token: char's int value == %d.",
                // __func__, src->lineno, c);
            // if (isprint(c))
                // fprintf(stderr, " char value: '%c'", c);
            // fprintf(stderr, "\n");
            src->token = TOK_UNKNOWN;
            break;
    }

    // Did we actually get a legal token?
    return src->token != TOK_UNKNOWN;
}

__attribute__((warn_unused_result))
static status_t savevalue(struct buffer *p)
{
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
            return failure;

        p->savedvalue = tmp;
        p->valuesize = n;
    }

    memcpy(p->savedvalue, p->value_start, n - 1);
    p->savedvalue[n - 1] = '\0'; // terminate the new string
    return success;
}

__attribute__((warn_unused_result))
static bool accept(struct buffer *src, enum tokentype tok)
{
    if (src->token == tok) {
        if (hasvalue(tok) && !savevalue(src))
            return false;

        if (nextsym(src))
            return true;
    }

    return false;
}

__attribute__((warn_unused_result))
static bool expect(struct buffer *p, enum tokentype tok)
{
    if (accept(p, tok))
        return true;

    // printf("%s(): expected token '%s', but found token around line %lu: %s\n",
    // __func__, maptoken(tok), p->lineno, maptoken(p->token));
    // printf("%s(): %s\n", __func__, p->savedvalue);
    return false;
}

// Notes on fail18.json:
// * Issue is when arrays nest too deep. Not sure why json has this
// limitation. (Maybe to avoid exploits trying to DOS the parser?)
// Anyway, we impose some limit. Test data indicates that 19 is max.
#define MAX_ARRAY_NESTING 19

__attribute__((warn_unused_result))
static struct value* accept_value(struct buffer *src)
{
    assert(src != NULL);

    if (accept(src, TOK_OBJECTSTART)) {
        list lst = accept_objects(src);
        if (!expect(src, TOK_OBJECTEND)) {
            list_free(lst, (dtor)object_free);
            return NULL;
        }

        return value_new(VAL_OBJECT, lst);
    }

    if (accept(src, TOK_ARRAYSTART)) {
        list lst = NULL;
        struct value *p;
        int ncommas = -1, nvalues = 0;

        // fail18: Can't nest too deep.
        src->narrays++;
        if (src->narrays > MAX_ARRAY_NESTING) {
            list_free(lst, (dtor)value_free);
            return NULL;
        }

        do {
            p = accept_value(src);

            // corner case: First element in array is missing. See fail6.json
            if (lst == NULL && p == NULL)
                break;

            if (p != NULL) {
                lst = list_add(lst, p);
                if (lst == NULL)
                    return NULL;

                nvalues++;
            }
            ncommas++;
        } while (accept(src, TOK_COMMA));

        // Note that we do need to be at token ARRAY END 
        if (!expect(src, TOK_ARRAYEND)) {
            list_free(lst, (dtor)value_free);
            return NULL;
        }
        src->narrays--;

        // handle fail4.json: The number of commas should equal
        // the number of values minus one.
        if (nvalues > 0 && nvalues - ncommas != 1) {
            list_free(lst, (dtor)value_free);
            return NULL;
        }

        // Wrap array list in a struct value object before returning 
        return value_new(VAL_ARRAY, lst);
    }
    
    if (accept(src, TOK_TRUE))
        return value_new(VAL_TRUE, NULL);
    
    if (accept(src, TOK_FALSE))
        return value_new(VAL_FALSE, NULL);
    
    if (accept(src, TOK_NULL))
        return value_new(VAL_NULL, NULL);
    
    if (accept(src, TOK_NUMBER)) {
        if (isinteger(src->savedvalue))
            return value_new(VAL_INTEGER, src->savedvalue);

        if (isreal(src->savedvalue))
            return value_new(VAL_DOUBLE, src->savedvalue);

        die("Internal error. Unhandled number from tokenizer");
    }
    
    if (accept(src, TOK_QSTRING))
        return value_new(VAL_QSTRING, src->savedvalue);
    
    if (accept(src, TOK_STRING))
        return value_new(VAL_STRING, NULL);
    
    if (accept(src, TOK_BOOLEAN))
        return value_new(VAL_BOOLEAN, NULL);

    return NULL;
}


// Accept one object, i.e., a name:value pair. 
// Opening brace HAS been read already
// Value may be null, as in { "foo" : { } }
__attribute__((warn_unused_result))
static struct object* accept_object(struct buffer *src)
{
    // Look for a name
    if (!accept(src, TOK_QSTRING))
        return NULL;

    // Name is now in the buffer's saved value. Create an object
    struct object *obj = object_new(src->savedvalue, NULL);

    // We need the : which separates name and value
    if (!expect(src, TOK_COLON))
        goto error;

    // Now get the value
    obj->value = accept_value(src);

    // It turns out that e.g. Google uses (key) as name, but without quotes.
    // This is not really JSON, but maybe it's OAS compliant?

    return obj;

error:
    object_free(obj);
    return NULL;
}

__attribute__((warn_unused_result))
static list accept_objects(struct buffer *src)
{
    list result;
    struct object *obj = NULL;
 
    if ((result = list_new()) == NULL)
        return NULL;

    do {
        if ((obj = accept_object(src)) == NULL)
            break;

        if (list_add(result, obj) == NULL)
            goto enomem;

    } while (accept(src, TOK_COMMA));

    return result;

enomem:
    list_free(result, (dtor)object_free);
    object_free(obj);
    return NULL;
}

__attribute__((warn_unused_result))
static status_t buffer_init(struct buffer *p, const void *src, size_t srclen)
{
    assert(p != NULL);
    assert(src != NULL);
    assert(srclen > 0);

    memset(p, 0, sizeof *p);
    p->mem = src;
    p->size = srclen;
    p->lineno = 1;
    p->narrays = 0;

    // pre-allocate the value buffer to avoid too many malloc/free cycles
    p->valuesize = 4096;
    if ((p->savedvalue = malloc(p->valuesize)) == NULL)
        return failure;

    return success;
}

static void buffer_cleanup(struct buffer *p)
{
    assert(p != NULL);

    free(p->savedvalue);
    p->savedvalue = NULL;
    p->valuesize = 0;
}

struct value* json_parse(const void *src, size_t srclen)
{
    struct buffer buf;
    struct value *val = NULL;

    assert(src != NULL);
    assert(srclen > 1); // Minimum is {}, as in nothing(object start and end).

    if (!buffer_init(&buf, src, srclen))
        return NULL;

    // Load first symbol
    if (!nextsym(&buf))
        goto error;

    // First symbol must be either array start or object start.
    if (buf.token != TOK_ARRAYSTART && buf.token != TOK_OBJECTSTART)
        goto error;

    val = accept_value(&buf);
    if (val == NULL)
        goto error;

    // Something's fucked if we still have tokens. See fail8.json
    // This is syntax error in input, not our error. We just need
    // to deal with it.
    if (nextsym(&buf))
        goto error;

    buffer_cleanup(&buf);
    return val;

error:
    buffer_cleanup(&buf);
    value_free(val);
    return NULL;
}

#ifdef JSON_CHECK

static void print_value(struct value *p);
static void json_traverse(struct value *value)
{
#if 1
    list_iterator i;
    list objects;

    assert(value != NULL);

    if (value->type == VAL_OBJECT) {
        objects = value->v.oval;
        if (objects == NULL)
            return; // null arrays, like [], are legal.

        puts("{ ");
        for (i = list_first(objects); !list_end(i); i = list_next(i)) {
            struct object *obj = list_get(i);
            if (obj->name != NULL) {
                printf("\"%s\" : ", obj->name);
                if (obj->value == NULL)
                    printf("(nullval)");
                else
                    print_value(obj->value);

                if (!list_last(i))
                    printf(",\n");
            }

            printf("\n");
        }
        puts(" }");
    }
    else if (value->type == VAL_ARRAY) {
        objects = value->v.aval;
        if (objects == NULL)
            return;

        puts("[ ");
        for (i = list_first(objects); !list_end(i); i = list_next(i)) {
            struct value *v = list_get(i);
            print_value(v);
            if (!list_last(i))
                printf(",\n");
        }
        puts(" ]");
    }

#else
    (void)objects;
#endif
}

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

static void print_objects(list lst)
{
    list_iterator i;

    assert(lst != NULL);
    printf("{\n");

    for (i = list_first(lst); !list_end(i); i = list_next(i)) {
        struct object *obj = list_get(i);
        if (obj == NULL) 
            printf("(no object in lst)");
        else
            print_object(obj);

        if (!list_last(i))
            printf(",\n");
    }

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
            printf("%g", p->v.dval);
            break;
    }
}

void json_free(struct value *objects)
{
    value_free(objects);
}

static int exitcode = 0;
static void testfile(const char *filename)
{
    int fd;
    struct stat st;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        die_perror(filename);

    if (fstat(fd, &st))
        die_perror(filename);

    const void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
        die_perror(filename);
    close(fd);

    struct value *objects = json_parse(mem, st.st_size);
    if (objects == NULL) {
        fprintf(stderr, "Unable to parse %s\n", filename);
        exitcode = 1;
    }

    if (1 && objects != NULL) json_traverse(objects);

    json_free(objects);
}

static void test_isinteger(void)
{
    static const struct {
        const char *value;
        bool result;
    } tests[] = {
        { "0" , true },
        { "-0" , true },
        { "-01" , false },
        { "-10" , true },
        { "1" , true },
        { "0123" , false },
        { "1000" , true },
        { "1X" , false },
        { "1.2" , false },
        { "-0.5" , false },
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
        { "1.2" , true },
        { "1.2e2" , true },
        { "1.2E2" , true },
        { "-0.5" , true },
        { "-0.5" , true },
        { "1000E10" , true },
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
        struct buffer buf;
        if (!buffer_init(&buf, tests[i].src, strlen(tests[i].src)))
            die("Unable to initialize buffer\n");

        int result = get_qstring(&buf);
        if (result != tests[i].result)
            die("%s(): src: <%s>: expected %s, got %s\n",
                __func__, tests[i].src, maptoken(tests[i].result),
                maptoken(result));

        ptrdiff_t blen = buf.value_end - buf.value_start;
        size_t reslen = strlen(tests[i].str);
        if ((size_t)blen != reslen)
            die("%s(): expected %lu bytes, got %ld\n",
                __func__, reslen, blen);

        int d = memcmp(tests[i].str, buf.value_start, reslen);
        if (d != 0)
            die("%s(): Something's really odd\n", __func__);

        buffer_cleanup(&buf);

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
            fprintf(stderr, "Testing contents of file %s\n", *argv);
            testfile(*argv);
        }
    }
    else {
        size_t i, n;

        n = sizeof filenames / sizeof *filenames;
        for (i = 0; i < n; i++) {
            fprintf(stderr, "Testing contents of file %s\n", filenames[i]);
            testfile(filenames[i]);
            fprintf(stderr, "\n");
        }
    }

    return exitcode;
}

#endif

