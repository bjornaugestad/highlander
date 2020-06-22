/* Simple JSON parser written by me. */
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "meta_common.h"
#include "meta_list.h"


// Been almost a year since I looked at this. Gotta start fresh...
// Some base rules
// * Objects start with { and end with }
// * Arrays start with [ and end with ]
// * stuff is name : value pairs, where name is a quoted string,
//   and value is one of: quoted string, number, true, false, null, array, object
// * array entries are also value, see above list of alternatives. No name in arrays though.
// OK, so we need some generic data structure to store everything in. We store
// one of the following: string, number, array, object, true|false|null. 
enum valuetype { VAL_UNKNOWN, VAL_STRING, VAL_INTEGER, VAL_ARRAY, VAL_OBJECT,
    VAL_TRUE, VAL_FALSE, VAL_NULL, VAL_DOUBLE };

enum tokentype { TOK_ERROR, TOK_UNKNOWN, TOK_QSTRING, TOK_TRUE, TOK_FALSE,
    TOK_COLON, TOK_COMMA, TOK_OBJECTSTART, TOK_OBJECTEND, TOK_ARRAYSTART,
    TOK_ARRAYEND, TOK_INTEGER, TOK_DOUBLE, TOK_NULL };


// We store our input buffer in one of these, to make it easier to 
// handle offset positions when we read from functions.
struct buffer {
    const char *mem;
    size_t nread, size;
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

struct pair {
    char *name;
    struct value *value;
};

static void value_free(struct value *p);

static void pair_free(struct pair *p)
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
        list_free(p->v.aval, (dtor)pair_free);
    else if (p->type == VAL_STRING)
        free(p->v.sval);
    else if (p->type == VAL_OBJECT)
        list_free(p->v.oval, (dtor)pair_free);

    free(p);
}

static struct pair* pair_new(const char *name, struct value *value)
{
    struct pair *p;

    assert(name != NULL);
    assert(value != NULL);

    if ((p = malloc(sizeof *p)) != NULL) {
        if ((p->name = strdup(name)) == NULL) {
            free(p);
            return NULL;
        }

        p->value = value;
    }

    return p;
}

static void buffer_dump(const struct buffer *p)
{
    size_t i;

    for (i = 0; i < p->nread; i++)
        fputc(p->mem[i], stderr);
}

static inline int parsebuf_getc(struct buffer *p)
{
    if (p->nread == p->size)
        return EOF;
    
    return p->mem[p->nread++];
}

static inline void parsebuf_ungetc(struct buffer *p)
{
    assert(p != NULL);
    assert(p->nread > 0);

    p->nread--;
}

// forward declarations because of recursive calls
static list accept_object(struct buffer *src);
static list accept_array(struct buffer *src);

#ifdef JSON_CHECK
// some member functions
static struct value * value_new(enum valuetype type, void *value)
{
    struct value *p;

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    p->type = type;
    switch (type) {
        case VAL_STRING:
            p->v.sval = strdup(value);
            break;

        case VAL_DOUBLE:
            p->v.dval = strtod(value, NULL);
            break;

        case VAL_INTEGER:
            p->v.lval = strtol(value, NULL, 10);
            break;

        case VAL_ARRAY:
            p->v.aval = list_add(NULL, value);
            if (p->v.aval == NULL)
                die("Meh, no mem");
            break;

        case VAL_OBJECT:
            p->v.oval = list_add(NULL, value);
            break;

        case VAL_UNKNOWN:
            die("Can't deal with unknown types");

        case VAL_TRUE:
        case VAL_FALSE:
        case VAL_NULL:
            // handled directly via type
            break;

        default:
            memset(&p->v, 0, sizeof p->v);
    }

    return p;
}

static const struct {
    int value;
    const char *text;
} tokens[] = {
    { TOK_ERROR       , "error" },
    { TOK_UNKNOWN     , "unknown" },
    { TOK_QSTRING     , "qstring" },
    { TOK_TRUE        , "true" },
    { TOK_FALSE       , "false" },
    { TOK_COLON       , "colon" },
    { TOK_COMMA       , "comma" },
    { TOK_OBJECTSTART , "objectstart" },
    { TOK_OBJECTEND   , "objectend" },
    { TOK_ARRAYSTART  , "arraystart" },
    { TOK_ARRAYEND    , "arrayend" },
    { TOK_INTEGER     , "integer" },
    { TOK_DOUBLE      , "double" },
    { TOK_NULL        , "null" },
};

static const char *maptoken(int value)
{
    size_t i, n;

    n = sizeof tokens / sizeof *tokens;
    for (i = 0; i < n; i++)
        if (tokens[i].value == value)
            return tokens[i].text;

    return "unknown token value";
}

// We have a ", which is start of quoted string. Read the rest of the
// string and place it in value. 
// I wonder if they escape quotes in json, for when one wants to transfer
// a quote as part of a value. I guess I'll find out...
// update: they do. see https://www.json.org/json-en.html for full syntax
//
// There's an error lurking here. \\" will be interpreted as \". TODO fix it
int get_string(struct buffer *src, char *value, size_t valuesize)
{
    size_t i = 0;
    int c, prev = 0;

    assert(src != NULL);
    assert(value != NULL);
    assert(valuesize > 0);

    while ((c = parsebuf_getc(src)) != EOF) {
        if (c == '"' && prev != '\\')
            break;

        prev = value[i++] = c;
    }

    if (i == valuesize)
        return TOK_ERROR;

    value[i] = '\0';

    return TOK_QSTRING;
}

// We've read a t. Are the next characters rue, as in true?
int get_true(struct buffer *src)
{
    if (parsebuf_getc(src) == 'r' 
    && parsebuf_getc(src) == 'u' 
    && parsebuf_getc(src) == 'e')
        return TOK_TRUE;

    return TOK_UNKNOWN;
}

int get_false(struct buffer *src)
{
    if (parsebuf_getc(src) == 'a' 
    && parsebuf_getc(src) == 'l' 
    && parsebuf_getc(src) == 's' 
    && parsebuf_getc(src) == 'e')
        return TOK_FALSE;

    return TOK_UNKNOWN;
}

int get_null(struct buffer *src)
{
    if (parsebuf_getc(src) == 'u' 
    && parsebuf_getc(src) == 'l' 
    && parsebuf_getc(src) == 'l')
        return TOK_NULL;

    return TOK_UNKNOWN;
}

// Read digits and place them in buffer, ungetc() the first non-digit
// so we can read it the next time.
int get_integer(struct buffer *src, char *value, size_t valuesize)
{
    size_t nread = 0;
    int c;

    assert(src != NULL);
    assert(value != NULL);
    assert(valuesize > 1);

    while ((c = parsebuf_getc(src)) != EOF) {
        if (isdigit(c)) {
            if (nread < valuesize)
                value[nread++] = c;
            else
                die("%s(): Buffer too small.\n", __func__);
        }
        else {
            value[nread] = '\0';
            parsebuf_ungetc(src);
            return TOK_INTEGER;
        }
    }

    // we get here on eof. That doesn't mean that we didn't read a number,
    // e.g. if the last token is this number.  Not very likely due 
    // to syntax requirements, so we EOF here always.
    return EOF;
}

int get_token(struct buffer *src, char *value, size_t valuesize)
{
    int c;

    while ((c = parsebuf_getc(src)) != EOF && isspace(c))
        ;

    switch (c) {
        case EOF: return EOF;
        case '[': return TOK_ARRAYSTART;
        case ']': return TOK_ARRAYEND;
        case '{': return TOK_OBJECTSTART;
        case '}': return TOK_OBJECTEND;
        case ':': return TOK_COLON;
        case ',': return TOK_COMMA;
        case '"': return get_string(src, value, valuesize);
    }

    // are we looking at a digit? If so, we need to retain its value
    // before we read more of the number.
    if (isdigit(c)) {
        value[0] = c;
        return get_integer(src, value + 1, valuesize - 1);
    }

    // At this point, we may have a true/false value, or even null
    if (c == 't')
        return get_true(src);
    else if (c == 'f')
        return get_false(src);
    else if (c == 'n')
        return get_null(src);
    else {
        fprintf(stderr, "c==%d\n", c);
        return TOK_UNKNOWN;
    }
}

static int accept_qstring(struct buffer *src, char *buf, size_t bufsize)
{
    assert(src != NULL);
    assert(buf != NULL);
    assert(bufsize > 0);

    return get_token(src, buf, bufsize);
}

// Wrap lists in a struct value object before returning lists
static struct value* accept_value(struct buffer *src)
{
    int c;
    char buf[2048];
    list lst;

    assert(src != NULL);

    c = get_token(src, buf, sizeof buf);
    switch (c) {
        case TOK_OBJECTSTART: 
            lst = accept_object(src);
            return value_new(VAL_OBJECT, lst);

        case TOK_ARRAYSTART: 
            lst = accept_array(src);
            return value_new(VAL_ARRAY, lst);

        // Hmm, we don't want to do anything here, do we?
        // case TOK_COMMA: return accept_value(src, buf, bufsize);

        case TOK_QSTRING: return value_new(VAL_STRING, buf);
        case TOK_TRUE:    return value_new(VAL_TRUE, buf);
        case TOK_FALSE:   return value_new(VAL_FALSE, buf);
        case TOK_INTEGER: return value_new(VAL_INTEGER, buf);
        case TOK_DOUBLE:  return value_new(VAL_DOUBLE, buf);
        case TOK_NULL:    return value_new(VAL_NULL, buf);
    }

    die("%s(): Meh, shouldn't be here\n", __func__);
}

static int accept_token(struct buffer *src)
{
    int c;
    char value[2048];
    
    value[0] = '\0';
    c = get_token(src, value, sizeof value);
    return c;
}

// Should we not return a list of all objects found? I think we should.
// Each entry in the list should be a name:value-pair known as a JSON object.
// boa@20200622
static list accept_object(struct buffer *src)
{
    int c;
    char name[2048];
    list result = list_new();
    struct value *value;
    struct pair *pr;
 
    // Keep reading name : value pairs until "}" is found
    for (;;) {
        name[0] = '\0';
        if ((c = accept_qstring(src, name, sizeof name)) != TOK_QSTRING) {
            if (c == TOK_OBJECTEND)
                break;

            buffer_dump(src);
            die("Expected name, got token %d/%s\n", c, maptoken(c));
        }

        if (accept_token(src) != TOK_COLON)
            die("Expected colon");

        if ((value = accept_value(src)) == NULL) {
            buffer_dump(src);
            fprintf(stderr, "Meh, could not get value for node with name %s\n", name);
            return NULL;
        }

        // add name and value to list
        if ((pr = pair_new(name, value)) == NULL || list_add(result, pr) == NULL)
            die("Out of memory\n");

        


        if (0) fprintf(stderr, "name:%s\n", name);
        if ((c = accept_token(src)) == TOK_OBJECTEND) 
            break;
        else if (c != TOK_COMMA) 
            die("Expected comma or object end, got %s", maptoken(c));
    }

    return result;
}

// Arrays can contain strings, numbers, objects, other arrays, and true,false,null
static list accept_array(struct buffer *src)
{
    int c;
    char value[2048];
    list result = list_new();
    list lst;
    struct value *val;

    assert(src != NULL);

    result = list_new();
    if (result == NULL)
        die("Out of memory");

    while ((c = get_token(src, value, sizeof value)) != EOF) {
        switch (c) {
            case TOK_TRUE:
                val = value_new(VAL_TRUE, value);
                if (list_add(result, val) == NULL)
                    die("out of memory\n");

                break;
            case TOK_FALSE:
                val = value_new(VAL_FALSE, value);
                if (list_add(result, val) == NULL)
                    die("out of memory\n");

                break;
            case TOK_NULL:
                val = value_new(VAL_NULL, value);
                if (list_add(result, val) == NULL)
                    die("out of memory\n");

                break;
            case TOK_INTEGER:
                val = value_new(VAL_INTEGER, value);
                if (list_add(result, val) == NULL)
                    die("out of memory\n");

                break;
            case TOK_DOUBLE:
                val = value_new(VAL_DOUBLE, value);
                if (list_add(result, val) == NULL)
                    die("out of memory\n");

                break;
            case TOK_QSTRING:
                val = value_new(VAL_STRING, value);
                if (list_add(result, val) == NULL)
                    die("out of memory\n");

                break;

            case TOK_COMMA:
                // Comma just separates array elements.
                break;

            case TOK_ARRAYEND:
                return result;

            case TOK_ARRAYSTART:
                if ((lst = accept_array(src)) == NULL)
                    die("%s(): Unable to read array\n", __func__);

                if (list_add(result, lst) == NULL)
                    die("Out of memory\n");

                break;

            case TOK_OBJECTSTART:
                lst = accept_object(src);
                if (list_add(result, lst) == NULL)
                    die("Out of memory\n");
                break;

            default:
                die("You gotta support all tokens, dude, even %d\n", c);
        }
    }

    die("If we get here, we miss an array end token\n");
    list_free(result, 0);
    return NULL;
}

static list parse(struct buffer *src)
{
    if (accept_token(src) != TOK_OBJECTSTART)
        die("Input must start with a {\n");

    return accept_object(src);
}

int main(void)
{
    const char *filename = "./schema.json";
    // const char *filename = "./xxx";
    int fd;
    struct stat st;
    struct buffer buf = { NULL, 0, 0 };
    list objects;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        die_perror(filename);

    if (fstat(fd, &st))
        die_perror(filename);

    buf.mem = mmap(NULL, buf.size = st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf.mem == MAP_FAILED)
        die_perror(filename);

    objects = parse(&buf);
    if (0) list_free(objects, (dtor)pair_free);

    close(fd);
    return 0;
}

#endif

