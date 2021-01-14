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


// Been almost a year since I looked at this. Gotta start fresh...
// Some base rules
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
    VAL_STRING, // This is not a quoted string, but the string keyword from oas 3.0.3
    VAL_BOOLEAN, // This is the boolean keyword from oas 3.0.3
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
    TOK_INTEGER,
    TOK_DOUBLE,
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
    char meh_bf_value[2048];

    // We don't want to copy values from the source to a temp
    // buffer, as we don't want to allocate memory, and we don't
    // want a fixed size buffer either. So we just point to
    // the start(first char) and end(last char) of the value we
    // found in the source memory(the mem pointer above)
    const char *value_start, *value_end;

    char *savedvalue;

    unsigned long lineno;
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

// name and value may be NULL
static struct object* object_new(const char *name, struct value *value)
{
    struct object *p;

    if ((p = calloc(1, sizeof *p)) != NULL) {
        if (name != NULL && (p->name = strdup(name)) == NULL) {
            free(p);
            return NULL;
        }

        p->value = value;
    }

    return p;
}

static inline const char* buffer_currpos(struct buffer *p)
{
    assert(p != NULL);
    return p->mem + p->nread;
}

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

// some member functions
static struct value * value_new(enum valuetype type, void *value)
{
    struct value *p;

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    p->type = type;
    switch (type) {
        case VAL_QSTRING:
            p->v.sval = strdup(value);
            break;

        case VAL_DOUBLE:
            p->v.dval = strtod(value, NULL);
            break;

        case VAL_INTEGER:
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
            die("Dude, wtf?\n");
    }

    return p;
}

static const struct {
    enum tokentype value;
    bool hasvalue;
    const char *text;
} tokens[] = {
    { TOK_ERROR       , false, "error" },
    { TOK_UNKNOWN     , false, "unknown" },
    { TOK_QSTRING     , true,  "qstring" },
    { TOK_TRUE        , false, "true" },
    { TOK_FALSE       , false, "false" },
    { TOK_COLON       , false, "colon" },
    { TOK_COMMA       , false, "comma" },
    { TOK_OBJECTSTART , false, "objectstart" },
    { TOK_OBJECTEND   , false, "objectend" },
    { TOK_ARRAYSTART  , false, "arraystart" },
    { TOK_ARRAYEND    , false, "arrayend" },
    { TOK_INTEGER     , true,  "integer" },
    { TOK_DOUBLE      , true,  "double" },
    { TOK_NULL        , false, "null" },
    { TOK_EOF         , false, "eof" },
    { TOK_STRING      , false, "string" },
};

static const char *maptoken(enum tokentype value)
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
int get_qstring(struct buffer *src)
{
    int c, prev = 0;

    assert(src != NULL);
    src->value_start = src->value_end = buffer_currpos(src);

    while ((c = buffer_getc(src)) != EOF) {
        if (c == '"' && prev != '\\')
            break;

        src->value_end++;
        prev = c;
    }

    return TOK_QSTRING;
}

// We've read a t. Are the next characters rue, as in true?
int get_true(struct buffer *src)
{
    if (buffer_getc(src) == 'r' 
    && buffer_getc(src) == 'u' 
    && buffer_getc(src) == 'e')
        return TOK_TRUE;

    return TOK_UNKNOWN;
}

int get_false(struct buffer *src)
{
    if (buffer_getc(src) == 'a' 
    && buffer_getc(src) == 'l' 
    && buffer_getc(src) == 's' 
    && buffer_getc(src) == 'e')
        return TOK_FALSE;

    return TOK_UNKNOWN;
}

int get_null(struct buffer *src)
{
    if (buffer_getc(src) == 'u' 
    && buffer_getc(src) == 'l' 
    && buffer_getc(src) == 'l')
        return TOK_NULL;

    return TOK_UNKNOWN;
}

int get_string(struct buffer *src)
{
    if (buffer_getc(src) == 't' 
    && buffer_getc(src) == 'r' 
    && buffer_getc(src) == 'i'
    && buffer_getc(src) == 'n'
    && buffer_getc(src) == 'g')
        return TOK_STRING;

    return TOK_UNKNOWN;
}

int get_boolean(struct buffer *src)
{
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
int get_integer(struct buffer *src)
{
    int c;

    assert(src != NULL);
    src->value_start = src->value_end = buffer_currpos(src);

    while ((c = buffer_getc(src)) != EOF) {
        if (isdigit(c) || c == '-') {
            src->value_end++;
        }
        else {
            src->value_end--;
            buffer_ungetc(src);

            if(0) printf("start:%p end %p\n", src->value_start, src->value_end);
            assert(src->value_end >= src->value_start);
            return TOK_INTEGER;
        }
    }

    // we get here on eof. That doesn't mean that we didn't read a number,
    // e.g. if the last token is this number.  Not very likely due 
    // to syntax requirements, so we EOF here always.
    return TOK_EOF;
}

static bool hasvalue(enum tokentype tok)
{
    size_t i, n = sizeof tokens / sizeof *tokens;

    for (i = 0; i < n; i++) {
        if (tokens[i].value == tok)
            return tokens[i].hasvalue;
    }

    die("Unknown token %d\n", (int)tok);
}

void get_token(struct buffer *src)
{
    int c;

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
        case '+':
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
            src->token = get_integer(src);
            break;

        default:
            fprintf(stderr, "%s():%lu: unexpected token: c==%d\n",
                __func__, src->lineno, c);
            src->token = TOK_UNKNOWN;
    }
}

static void nextsym(struct buffer *p)
{
    if (p->token != TOK_EOF)
        get_token(p);
}

static void savevalue(struct buffer *p)
{
    assert(p != NULL);
    assert(p->value_start != NULL);
    assert(p->value_end != NULL);
    assert(p->value_end >= p->value_start); // At least one char makes sense

    free(p->savedvalue);

    size_t n = p->value_end - p->value_start + 1;
    if ((p->savedvalue = malloc(n)) == NULL)
        die("Out of memory\n");

    memcpy(p->savedvalue, p->value_start, n - 1);
    p->savedvalue[n - 1] = '\0'; // terminate the new string
}

static bool accept(struct buffer *src, enum tokentype tok)
{
    if (src->token == tok) {
        if (hasvalue(tok))
            savevalue(src);

        nextsym(src);
        return true;
    }

    return false;
}

static bool expect(struct buffer *p, enum tokentype tok)
{
    if (accept(p, tok))
        return true;

    die("%s(): expected token '%s', but unexpected token found around line %lu: %s\n",
        __func__, maptoken(tok), p->lineno, maptoken(p->token));
    return false;
}

// Note that array handling is buggy atm boa@20210114
//
static struct value* accept_value(struct buffer *src)
{
    assert(src != NULL);

    if (accept(src, TOK_OBJECTSTART)) {
        list lst = accept_objects(src);
        expect(src, TOK_OBJECTEND);
        return value_new(VAL_OBJECT, lst);
    }
    else if (accept(src, TOK_ARRAYSTART)) {
        // Wrap array list in a struct value object before returning 
        list lst = NULL;
        do {
            struct value *p = accept_value(src);
            if (p != NULL)
                lst = list_add(lst, p);
        } while (accept(src, TOK_COMMA));

        // Note that we do need to be at token ARRAY END 
        expect(src, TOK_ARRAYEND);

        return value_new(VAL_ARRAY, lst);
    }
    else if (accept(src, TOK_TRUE)) {
        return value_new(VAL_TRUE, NULL);
    }
    else if (accept(src, TOK_FALSE)) {
        return value_new(VAL_FALSE, NULL);
    }
    else if (accept(src, TOK_NULL)) {
        return value_new(VAL_NULL, NULL);
    }
    else if (accept(src, TOK_INTEGER)) {
        return value_new(VAL_INTEGER, src->savedvalue);
    }
    else if (accept(src, TOK_DOUBLE)) {
        return value_new(VAL_DOUBLE, src->savedvalue);
    }
    else if (accept(src, TOK_QSTRING)) {
        return value_new(VAL_QSTRING, src->savedvalue);
    }
    else if (accept(src, TOK_STRING)) {
        return value_new(VAL_STRING, NULL);
    }
    else if (accept(src, TOK_BOOLEAN)) {
        return value_new(VAL_BOOLEAN, NULL);
    }

    return NULL;
}


// Accept one object, i.e., a name:value pair. Opening brace HAS been read already
// Value may be null, as in { "foo" : { } }
static struct object* accept_object(struct buffer *src)
{
    // Look for a name
    if (!accept(src, TOK_QSTRING))
        return NULL;

    // Name is now in the buffer's saved value. Create an object
    struct object *obj = object_new(src->savedvalue, NULL);

    // We need the : which separates name and value
    expect(src, TOK_COLON);

    // Now get the value
    obj->value = accept_value(src);

    // It turns out that e.g. Google uses (key) as name, but without quotes.
    // This is not really JSON, but maybe it's OAS compliant?

    return obj;
}

static list accept_objects(struct buffer *src)
{
    list result;
 
    if ((result = list_new()) == NULL)
        die("Out of memory");

    do {
        struct object *obj;

        if ((obj = accept_object(src)) == NULL)
            break;

        if (list_add(result, obj) == NULL)
            die("Out of memory");

    } while (accept(src, TOK_COMMA));

    return result;
}

#ifdef JSON_CHECK
static list json_parse(struct buffer *src)
{
    list result;
    nextsym(src);

    if (!accept(src, TOK_OBJECTSTART))
        die("Input must start with a {\n");

    result = accept_objects(src);
    expect(src, TOK_OBJECTEND);
    expect(src, TOK_EOF);
    return result;
}

static void print_value(struct value *p);
static void json_traverse(list objects)
{
    list_iterator i;

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

static void buffer_init(struct buffer *p)
{
    memset(p, 0, sizeof *p);
    p->lineno = 1;
}

static void testfile(const char *filename)
{
    int fd;
    struct stat st;
    struct buffer buf;
    list objects;

    buffer_init(&buf);
    fd = open(filename, O_RDONLY);
    if (fd == -1)
        die_perror(filename);

    if (fstat(fd, &st))
        die_perror(filename);

    buf.mem = mmap(NULL, buf.size = st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buf.mem == MAP_FAILED)
        die_perror(filename);

    objects = json_parse(&buf);
    if (0) json_traverse(objects);
    if (1) list_free(objects, (dtor)object_free);
    free(buf.savedvalue);

    close(fd);
}

int main(int argc, char *argv[])
{
    const char *filenames[] = {
        "./array_with_no_entries.json",
        "./array_with_one_entry.json",
        "./schema.json",
        "./github/ghes-3.0.json"
    };

    if (argc > 1) {
        while (*++argv != NULL)
            testfile(*argv);
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

    
    return 0;
}

#endif

