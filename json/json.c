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
#define VAL_UNKNOWN 0
#define VAL_STRING  1
#define VAL_INTEGER 2
#define VAL_ARRAY   3
#define VAL_OBJECT  4
#define VAL_TRUE    5
#define VAL_FALSE   6
#define VAL_NULL    7
#define VAL_DOUBLE  8


// We store our input buffer in one of these, to make it easier to 
// handle offset positions when we read from functions.
struct parsebuf {
    const char *mem;
    size_t nread, size;
};

static void parsebuf_dump(const struct parsebuf *p)
{
    size_t i;
    for (i = 0; i < p->nread; i++)
        fputc(p->mem[i], stderr);
}

static inline int parsebuf_getc(struct parsebuf *p)
{
    if (p->nread == p->size)
        return EOF;
    
    return p->mem[p->nread++];
}

static inline void parsebuf_ungetc(struct parsebuf *p)
{
    assert(p != NULL);
    assert(p->nread > 0);

    p->nread--;
}

struct node {
    // Which value type are we storing here?
    int type;

    // name, as in name/value pair.
    char *name;

    union {
        char *sval; // string
        long lval;  // long integers
        double dval; // floating point numbers
        list aval; // array value, all struct node-pointers.
        struct node *oval; // object value. 
    } v;
};

// forward declarations because of recursive calls
static struct node* accept_object(struct parsebuf *src);
static struct node* accept_array(struct parsebuf *src);

#ifdef JSON_CHECK
// some member functions
static struct node * node_new(int type, const char *name, void *value)
{
    struct node *p;

    assert(name != NULL);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if ((p->name = strdup(name)) == NULL) {
        free(p);
        return NULL;
    }

    p->type = type;
    switch (type) {
        case VAL_STRING : p->v.sval = strdup(value); break;
        case VAL_INTEGER: p->v.lval = (long)value; break;
        case VAL_ARRAY  :
            p->v.aval = list_add(NULL, value);
            if (p->v.aval == NULL)
                die("Meh, no mem");
            break;
        case VAL_OBJECT : p->v.oval = value; break;
        default:
            memset(&p->v, 0, sizeof p->v);
    }

    return p;
}

// Add src to dest's list of nodes. From now on the dest node owns
// the memory. That's why src is non-const.
static status_t node_add(struct node *dest, struct node *src)
{
    assert(dest != NULL);
    assert(dest->type == VAL_ARRAY);
    assert(src != NULL);
    
    if (list_add(dest->v.aval, src) == NULL)
        return failure;

    return success;
}

// Set a value type and value for an existing node.
static void node_set(struct node *p, int type, void *value)
{
    assert(p != NULL);
    assert(value != NULL);

    p->type = type;
    switch (type) {
        case VAL_STRING : p->v.sval = strdup(value); break;
        case VAL_INTEGER: p->v.lval = (long)value; break;
        case VAL_ARRAY  :
            p->v.aval = list_add(NULL, value);
            if (p->v.aval == NULL)
                die("Meh, no mem");
            break;
        case VAL_OBJECT : p->v.oval = value; break;
        default:
            die("Internal error. Unsupported type");
    }

}
static void node_free(struct node *p)
{
    if (p == NULL)
        return;

    free(p->name);
    if (p->type == VAL_ARRAY)
        list_free(p->v.aval, (dtor)node_free); // wait with dtor
    else if (p->type == VAL_STRING)
        free(p->v.sval);
    else if (p->type == VAL_OBJECT)
        node_free(p->v.oval);
    free(p);
}


// We have some tokens which we can return
#define TOK_ERROR      -1
#define TOK_UNKNOWN     0
#define TOK_QSTRING     1
#define TOK_TRUE        2
#define TOK_FALSE       3
#define TOK_COLON       4
#define TOK_COMMA       5
#define TOK_OBJECTSTART 6
#define TOK_OBJECTEND   7
#define TOK_ARRAYSTART  8
#define TOK_ARRAYEND    9
#define TOK_INTEGER    10
#define TOK_DOUBLE     11
#define TOK_NULL       12

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
int get_string(struct parsebuf *src, char *value, size_t valuesize)
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
int get_true(struct parsebuf *src)
{
    if (parsebuf_getc(src) == 'r' 
    && parsebuf_getc(src) == 'u' 
    && parsebuf_getc(src) == 'e')
        return TOK_TRUE;

    return TOK_UNKNOWN;
}

int get_false(struct parsebuf *src)
{
    if (parsebuf_getc(src) == 'a' 
    && parsebuf_getc(src) == 'l' 
    && parsebuf_getc(src) == 's' 
    && parsebuf_getc(src) == 'e')
        return TOK_FALSE;

    return TOK_UNKNOWN;
}

int get_null(struct parsebuf *src)
{
    if (parsebuf_getc(src) == 'u' 
    && parsebuf_getc(src) == 'l' 
    && parsebuf_getc(src) == 'l')
        return TOK_NULL;

    return TOK_UNKNOWN;
}

// Read digits and place them in buffer, ungetc() the first non-digit
// so we can read it the next time.
int get_integer(struct parsebuf *src, char *value, size_t valuesize)
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

int get_token(struct parsebuf *src, char *value, size_t valuesize)
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

static int accept_qstring(struct parsebuf *src, char *buf, size_t bufsize)
{
    assert(src != NULL);
    assert(buf != NULL);
    assert(bufsize > 0);

    return get_token(src, buf, bufsize);
}

static struct node* accept_value(struct parsebuf *src, char *buf, size_t bufsize)
{
    int c;
    struct node *p;

    assert(src != NULL);
    assert(buf != NULL);
    assert(bufsize > 0);

    c = get_token(src, buf, bufsize);
    switch (c) {
        case TOK_OBJECTSTART: 
            p = accept_object(src);
            break;

        case TOK_ARRAYSTART: 
            p = accept_array(src);
            break;

        // Hmm, we don't want to do anything here, do we?
        // case TOK_COMMA: return accept_value(src, buf, bufsize);

        case TOK_QSTRING:
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_INTEGER:
        case TOK_DOUBLE:
        case TOK_NULL:
            return node_new(c, "val", buf);
    }

    return p;
    die("%s(): Meh, shouldn't be here\n", __func__);
}

static int accept_token(struct parsebuf *src)
{
    int c;
    char value[2048];
    
    value[0] = '\0';
    c = get_token(src, value, sizeof value);
    return c;
}

static struct node* accept_object(struct parsebuf *src)
{
    int c;
    char name[2048], value[2048];
    struct node *result = NULL;
 
    (void)node_set;
    // Keep reading name : value pairs until "}" is found
    for (;;) {
        name[0] = '\0';
        if ((c = accept_qstring(src, name, sizeof name)) != TOK_QSTRING) {
            if (c == TOK_OBJECTEND)
                break;

            parsebuf_dump(src);
            die("Expected name, got token %d/%s\n", c, maptoken(c));
        }

        if (accept_token(src) != TOK_COLON)
            die("Expected colon");

        value[0] = '\0';
        result = accept_value(src, value, sizeof value);
        if (result == NULL)
            die("%s(): Unable to read value\n", __func__);

        if ((c = accept_token(src)) == TOK_OBJECTEND) {
            break;
        }
        else if (c == TOK_COMMA) {
            // read next name:value pair in next iteration
        }
        else
            die("Expected comma or object end, got %s", maptoken(c));
    }

    return result;
}

// Arrays can contain strings, numbers, objects, other arrays, and true,false,null
static struct node* accept_array(struct parsebuf *src)
{
    int c;
    char value[2048];
    struct node *result, *p;

    assert(src != NULL);

    result = node_new(VAL_ARRAY, "arrname", NULL);
    if (result == NULL)
        die("Out of memory");

    while ((c = get_token(src, value, sizeof value)) != EOF) {
        switch (c) {
            case TOK_TRUE:
            case TOK_FALSE:
            case TOK_NULL:
            case TOK_INTEGER:
            case TOK_DOUBLE:
            case TOK_QSTRING:
                p = node_new(c, "val", value);
                node_add(result, p);
                break;

            case TOK_COMMA:
                // Comma just separates array elements.
                break;

            case TOK_ARRAYEND:
                return result;

            case TOK_ARRAYSTART:
                p = accept_array(src);
                if (p == NULL)
                    die("%s(): Unable to read array\n", __func__);

                node_add(result, p);
                break;

            case TOK_OBJECTSTART:
                result = accept_object(src);
                break;

            default:
                die("You gotta support all tokens, dude, even %d\n", c);
        }
    }

    die("If we get here, we miss an array end token\n");
    node_free(result);
    return NULL;
}

static struct node* parse(struct parsebuf *src)
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
    struct parsebuf buf = { NULL, 0, 0 };

    struct node *root = node_new(VAL_OBJECT, "xx", NULL);

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        die_perror(filename);

    if (fstat(fd, &st))
        die_perror(filename);

    buf.mem = mmap(NULL, buf.size = st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf.mem == MAP_FAILED)
        die_perror(filename);

    parse(&buf);

    close(fd);
    node_free(root);
    return 0;
}

#endif

