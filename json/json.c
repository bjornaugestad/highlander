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

static inline int parsebuf_getc(struct parsebuf *p)
{
    if (p->nread == p->size)
        return EOF;
    
    return p->mem[p->nread++];
}

struct node {
    // Which value type are we storing here?
    int type;

    // name, as in name/value pair.
    // Kinda optional so we can reuse this struct for all values.
    char *name;

    union {
        char *sval; // string
        long lval;  // long integers
        double dval; // floating point numbers
        list aval; // array value, all struct node-pointers.
        struct node *oval; // object value. 
    } v;
};

#ifdef JSON_CHECK
// some member functions
static struct node * node_new(int type, const char *name, void *value)
{
    struct node *p;

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if (name == NULL) {
        p->name = NULL;
    }
    else if ((p->name = strdup(name)) == NULL) {
        free(p);
        return NULL;
    }

    p->type = type;
    switch (type) {
        case VAL_STRING : p->v.sval = strdup(value); break;
        case VAL_INTEGER: p->v.lval = (long)value; break;
        case VAL_ARRAY  : p->v.aval = value; break;
        case VAL_OBJECT : p->v.oval = value; break;
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

// We have a ", which is start of quoted string. Read the rest of the
// string and place it in value. 
// I wonder if they escape quotes in json, for when one wants to transfer
// a quote as part of a value. I guess I'll find out...
// update: they do. see https://www.json.org/json-en.html for full syntax
int get_string(struct parsebuf *src, char *value, size_t valuesize)
{
    size_t i = 0;
    int c;

    assert(src != NULL);
    assert(value != NULL);
    assert(valuesize > 0);

    while ((c = parsebuf_getc(src)) != EOF && c != '"') {
        value[i++] = c;
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

// static int expect(int token, char *value, size_t valuesize);


// Can we tokenize the json input? Let's try.
// * Most stuff is quoted strings. Some values are unquoted strings, like true and false
// * We must handle {}[]:,
// * Things nest a lot, so we need some tree/stack model eventually. It nests on braces,
// so we can push things to a stack on { and pop it on }. 
// * We have enum lists between [ and ]. Some of these are simple "list of strings",
//   but not all. Some are named "oneOf" and have multiple(two) alternatives. Each
//   alternative is a block. So
//      "oneof": [ { "foo": "bar" }, { "baz" : "blob" } ]
//   happens to be a legal value. 
static struct node* parse(struct parsebuf *src, struct node *root)
{
    int token;
    char name[2048], value[2048] = "";
    // struct node *p = root;

    #define STATE_UNKNOWN 0
    #define STATE_OBJECT  1
    #define STATE_ARRAY   2
    int state = STATE_UNKNOWN;

    // Save name, if any, so we can use it when calling node_new().
    // Note that name is required for objects(qstring : qstring),
    // so we need to know if current context is object/array/
    // We can feed that into get_token() as well, and distinguish
    // between different types of QSTRINGs

    while ((token = get_token(src, value, sizeof value)) != EOF) {

        if (state == STATE_OBJECT) {
            strcpy(name, value); // save it for later
        }
        else if (state == STATE_ARRAY) {
            // We have a new array element. Add it to current node
        }

        switch (token) {
            case TOK_QSTRING:
                break;

            case TOK_COLON:
                break;

            case TOK_COMMA:
                break;

            case TOK_OBJECTSTART:
                state = STATE_OBJECT;
                parse(src, root);
                break;

            case TOK_OBJECTEND:
                return root;

            case TOK_ARRAYSTART:
                state = STATE_ARRAY;
                parse(src, root);
                break;

            case TOK_ARRAYEND:
                return root;
                
            case TOK_TRUE:
            case TOK_FALSE:
            case TOK_INTEGER:
            case TOK_DOUBLE:
            case TOK_NULL:
                break;

            case TOK_ERROR:
            case TOK_UNKNOWN:
            default:
                fprintf(stderr, "meh, didn't handle token %d\n", token);
                exit(EXIT_FAILURE);
                break;
        }
    }

    return NULL;
}

int main(void)
{
    const char *filename = "./schema.json";
    int fd;
    struct stat st;
    struct parsebuf buf = { NULL, 0, 0 };

    struct node *root = node_new(VAL_OBJECT, NULL, NULL);

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        die_perror(filename);

    if (fstat(fd, &st))
        die_perror(filename);

    buf.mem = mmap(NULL, buf.size = st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf.mem == MAP_FAILED)
        die_perror(filename);

    parse(&buf, root);

    close(fd);
    node_free(root);
    return 0;
    (void)node_add; // to avoid warning/error
}

#endif

