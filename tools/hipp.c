/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

/**
 * hipp - The highlander preprocessor.
 * hipp converts HTML files to C files and supports embedded scripting
 * of C code. It is a simple program and works approximately like this:
 * - hipp looks for the tags <% and %>.
 * - everything OUTSIDE these tags are converted to a const char*[]
 *	 with one entry per line.
 * - everything INSIDE these tags are left untouched.
 * - A function suitable for the Highlander web server is created.
 *
 * You can also run the hipp in a special prototype mode, which will
 * generate function declarations for all pages. These functions should
 * be stored in an include file suitable for inclusion in a C or C++
 * source file.
 *
 * A special section at the top of the file is also understood. There
 * may exist a need to add code outside the generated function, e.g.
 * include-statements or local functions/variables. That can be done
 * within the tags <[ and ]>. Note that these tags MUST be at the
 * very beginning of the file.
 *
 * Limits:
 * Line length: 2048
 *
 * Version: 0.0.1
 * Author : bjorn@augestad.online
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Line number in current file */
char* g_current_file = NULL;
char* g_headerfile = NULL;
char* g_outputfile = NULL;
char* g_mainfile = NULL;
const char *g_content_type = NULL;

static int m_skip_line_numbers = 0;
static int prototype_mode = 0;
static int m_strip_blanks = 0;
static int m_automake = 0;
static int lineno = 1;

/* local helper functions */
static void print_line_directive(FILE *f, const char* file, int line);
static void show_help(void);
static void process_file(char* name);
static void create_header(char* filename, int argc, char *argv[], int opt_ind);
static void write_html_buffer(FILE* fout, const char* html);
static void create_mainfile(int argc, char *argv[], const char *filename);
static void create_autoxx_files(int argc, char *argv[]);


static void p(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
}

/* Print array of strings, add newline. */
static void parr(FILE *f, const char *arr[], size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        fprintf(f, "%s\n", arr[i]);
}

int main(int argc, char *argv[])
{
    int i, c;
    extern int optind;
    extern char* optarg;

    while ( (c = getopt(argc, argv, "Am:t:o:i:Eshp")) != EOF) {
        switch (c) {
            case 'A':
                m_automake = 1;
                break;

            case 't':
                g_content_type = optarg;
                break;

            case 'o':
                g_outputfile = optarg;
                break;

            case 'i':
                g_headerfile = optarg;
                break;

            case 'm':
                g_mainfile = optarg;
                break;

            case 'h':
                show_help();
                exit(EXIT_SUCCESS);
                break;

            case 'E':
                m_skip_line_numbers = 1;
                break;

            case 's':
                m_strip_blanks = 1;
                break;

            case 'p':
                prototype_mode = 1;
                break;

            default:
                fprintf(stderr, "hipp: Unknown parameter\n");
                exit(EXIT_FAILURE);
        }
    }

    if (g_headerfile == NULL) {
        fprintf(stderr, "%s: Required argument -i is missing.\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (optind == argc) {
        fprintf(stderr, "hipp: No input files\n");
        exit(EXIT_FAILURE);
    }

    if (g_outputfile != NULL && optind + 1 < argc) {
        fprintf(stderr, "-o option is only valid if input is one file\n");
        exit(EXIT_FAILURE);
    }

    if (prototype_mode)
        create_header(g_headerfile, argc, argv, optind);
    else {
        /* Generate C code for all the files */
        for (i = optind; i < argc; i++)
            process_file(argv[i]);
    }


    /*
     * New stuff 20070401:
     * -m can be used to generate a skeleton main function
     * which sets up a web server for us.
     */
    if (g_mainfile != NULL)
        create_mainfile(argc - optind, &argv[optind], g_mainfile);

    if (m_automake)
        create_autoxx_files(argc - optind, &argv[optind]);


    exit(EXIT_SUCCESS);
}

static void show_help(void)
{
    printf("Please see the man page for detailed help\n");
    printf("\n");
}

// Create a legal C name, but do NOT add more than one consecutive underscore.
static const char* legal_name(const char* base)
{
    char *s;
    static char name[1024];
    int last_was_underscore = 0;

    /* Skip all leading illegal chars. */
    while (*base != '\0' && !isalnum(*base))
        base++;

    assert(*base != '\0');

    /* Create legal name */
    s = name;
    while (*base != '\0') {
        if (!isalnum(*base)) {
            if (!last_was_underscore) {
                *s++ = '_';
                last_was_underscore = 1;
            }
        }
        else {
            *s++ = *base;
            last_was_underscore = 0;
        }

        base++;
    }

    *s = '\0';
    return name;
}

/* call this function with full path.
 * It creates *the* function name we want to use.
 */
static const char* function_name(char* file)
{
    static char name[10240];

    assert(file != NULL);

    strcpy(name, "hipp_");
    strcat(name, legal_name(file));
    return name;
}

/**
 * Adds standard stuff to all files.
 * Stuff we need is:
 * Standard C header files
 * The Highlander header file
 * Copyright info?
 * License? (Naaah..)
 */
static void print_standard_stuff(FILE* f)
{
    static const char* headers =
        "#include <assert.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <unistd.h>\n"
        "\n"
        "#include <highlander.h>\n"
        ;


    p(f, "%s\n", headers);
    p(f, "#include \"%s\"\n", g_headerfile);
    p(f, "\n");

}

static void print_fn(FILE*f, char* name)
{
    const char* fn = function_name(name);

    p(f, "int %s(http_request request, http_response response)\n", fn);
    p(f, "{\n");
    p(f, "\tassert(request != NULL);\n");
    p(f, "\tassert(response != NULL);\n");
    p(f, "\n");

    if (g_content_type != NULL) {
        p(f, "\tif (!response_set_content_type(response, \"%s\"))\n", g_content_type);
        p(f, "\t\treturn HTTP_500_INTERNAL_SERVER_ERROR;\n");
        p(f, "\n");
    }
}

static void write_html_buffer(FILE* f, const char* str)
{
    int printed = 0;
    const char *s;
    char last = '\0';
    const char* wrappers = "> ";

    s = str;
    p(f, "\t{\n");
    p(f, "\t\tconst char* html = \n");
    p(f, "\t\t\"");
    while (*s) {
        if (isspace(*s) && m_strip_blanks && printed == 0) {
            s++;
            continue;
        }
        else if (*s == '\\') {
            p(f, "\\\\");
        }
        else if (*s == '\n') {
            if (m_strip_blanks) {
                p(f, "\\n");
            }
        }
        else if (*s == '"' && last != '\\')
            p(f, "\\%c", *s);
        else
            fputc(*s, f);


        printed++;
        if (*s == '\n'
        || (last != '\\' && strchr(wrappers, *s) && printed > 70)) {
            printed = 0;
            p(f, "\\n\"\n\t\t\"");
            last = '\0';
        }
        else
            last = *s;

        s++;
    }

    p(f, "\";\n");
    p(f, "\t\tif (!response_add(response, html))\n");
    p(f, "\t\t\treturn HTTP_500_INTERNAL_SERVER_ERROR;\n");
    p(f, "\t}\n");
}

static const char* remove_ext(char* name)
{
    static char buf[10000];
    char *s;

    strcpy(buf, name);
    s = buf + strlen(buf) - 1;
    while (s != buf) {
        if (*s == '.') {
            *s = '\0';
            break;
        }
        s--;
    }

    if (s == buf) {
        fprintf(stderr, "hipp: internal error\n");
        exit(EXIT_FAILURE);
    }

    return buf;
}

static int peek(FILE* f)
{
    int c = fgetc(f);
    ungetc(c, f);
    return c;
}

static void append(char *s, char c)
{
    char tmp[2];

    tmp[0] = c;
    tmp[1] = '\0';
    strcat(s, tmp);
}

char buf[1024 * 1024];

static void process_file(char* name)
{
    FILE *fin, *fout;
    char fname[10240];
    char *base;
    int in_header = 0, in_hipp = 0;
    int c;
    int fn_written = 0;


    /* buffers html */
    buf[0] = '\0';

    if ((base = basename(name)) == NULL) {
        perror(name);
        exit(EXIT_FAILURE);
    }

    if (g_outputfile != NULL)
        strcpy(fname, g_outputfile);
    else {
        strcpy(fname, remove_ext(base));
        strcat(fname, ".c");
    }

    lineno = 1;
    g_current_file = name;
    if ((fin = fopen(name, "r")) == NULL) {
        perror(name);
        exit(EXIT_FAILURE);
    }
    else if ((fout = fopen(fname, "w")) == NULL) {
        perror(fname);
        exit(EXIT_FAILURE);
    }

    print_standard_stuff(fout);
    print_line_directive(fout, g_current_file, lineno);

    while ( (c = fgetc(fin)) != EOF) {
        switch (c) {
            case '<':
                if (peek(fin) == '%') {
                    if (in_hipp || in_header) {
                        fprintf(stderr, "hipp: Tags do not nest\n");
                        exit(EXIT_FAILURE);
                    }

                    /* We're entering hipp mode, print buf */
                    in_hipp = 1;
                    if (!fn_written) {
                        print_fn(fout, name);
                        fn_written = 1;
                    }

                    write_html_buffer(fout, buf);
                    buf[0] = '\0';

                    print_line_directive(fout, g_current_file, lineno);
                    fgetc(fin); /* Skip % */
                }
                else
                    append(buf, c);

                break;

            case '%':
                if (peek(fin) == '>') {
                    /* We're leaving hipp code. Print it */
                    in_hipp = 0;
                    p(fout, "%s", buf);
                    buf[0] = '\0';
                    fgetc(fin); /* Skip > */
                }
                else if (peek(fin) == '{') {
                    if (in_hipp || in_header) {
                        fprintf(stderr, "hipp: Tags do not nest\n");
                        exit(EXIT_FAILURE);
                    }

                    /* We're entering header mode */
                    in_header = 1;
                    buf[0] = '\0';
                    fgetc(fin); /* Skip { */
                }
                else if (peek(fin) == '}') {
                    /* We're leaving header mode */
                    in_header = 0;
                    p(fout, "%s", buf);
                    buf[0] = '\0';
                    fgetc(fin); /* Skip } */
                }
                else {
                    append(buf, '%');
                }

                break;

            default:
                if (c == '\n')
                    lineno++;

                /* Add character to buffer */
                append(buf, c);
                break;
        }
    }

    if (!fn_written)
        print_fn(fout, name);

    if (strlen(buf) > 0)
        write_html_buffer(fout, buf);

    p(fout, "\treturn 0;\n");
    p(fout, "}\n");


    fclose(fin);
    fclose(fout);
}

static void print_line_directive(FILE *f, const char* file, int line)
{
    if (!m_skip_line_numbers)
        p(f, "#line %d \"%s\"\n", line, file);
}

static void create_header(char* filename, int argc, char *argv[], int opt_ind)
{
    int i;
    FILE* f;
    char *base, *s, guard[2048];

    if ((base = basename(filename)) == NULL) {
        fprintf(stderr, "%s: Could not get basename\n", filename);
        exit(EXIT_FAILURE);
    }

    strcpy(guard, remove_ext(base));
    s = guard;
    while (*s != '\0')  {
        *s = toupper((unsigned char)*s);
        s++;
    }

    if ( (f = fopen(filename, "w")) == NULL) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    p(f, "#ifndef HIPP_%s_H\n", guard);
    p(f, "#define HIPP_%s_H\n", guard);

    p(f, "\n");
    p(f, "#include <highlander.h>\n");
    p(f, "\n");

    p(f, "\n");
    p(f, "#ifdef __cplusplus\n");
    p(f, "extern \"C\" {\n");
    p(f, "#endif\n");
    p(f, "\n");


    for (i = opt_ind; i < argc; i++)
        p(f, "int %s(http_request request, http_response response);\n", function_name(argv[i]));

    p(f, "\n");
    p(f, "#ifdef __cplusplus\n");
    p(f, "}\n");
    p(f, "#endif\n");
    p(f, "\n");
    p(f, "#endif /* guard */\n");

    fclose(f);
}

/*
 * Creates the main function and saves it in filename.
 * Now, what goes into the main function?
 * #include statements
 * a http_server object
 * Adding all the function handlers for the pages
 * Cleanup
 */
static void create_mainfile(int argc, char *argv[], const char *filename)
{
    int i;
    FILE *f;

    if ( (f = fopen(filename, "w")) == NULL) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    p(f, "#include <stdlib.h>\n");
    p(f, "#include <unistd.h>\n");
    p(f, "#include <errno.h>\n");
    p(f, "#include <highlander.h>\n");
    p(f, "#include <meta_process.h>\n");
    p(f, "#include <meta_common.h>\n");
    p(f, "\n");
    p(f, "#include \"%s\"\n", g_headerfile);
    p(f, "\n");

    static const char *mainfunc[] = {
    "int main(int argc, char *argv[])",
    "{",
    "\t// Name of running process",
    "\tconst char *appname = \"foo\";",
    "",
    "\t// application's root/working directory",
    "\tconst char *rootdir = \"/tmp\";",
    "",
    "\t// application runs with this user's privileges",
    "\tconst char *user = \"nobody\";",
    "",
    "\thttp_server s;",
    "\tprocess proc;",
    "\tint fork_and_close = 0;",
    "",
    "\t// Bind and listen to host/port.",
    "\tstatic const char *hostname = \"localhost\";",
    "\tint portnumber = 2000;",
    "",
    "\t/* Silence the compiler */",
    "\t(void)argc;",
    "\t(void)argv;",
    "",
    "\t/* Enable debug output from the debug() function. */",
    "\tmeta_enable_debug_output();",
    "",
    "\t/* Print a test line. */",
    "\tdebug(\"Here we go\\n\");",
    "",
    "\t/* First we create the web server and the process */",
    "\tif( (s = http_server_new()) == NULL)",
    "\t\tdie(\"Could not create http server.\\n\");",
    "",
    "\tif( (proc = process_new(appname)) == NULL)",
    "\t\tdie(\"Could not create process object.\\n\");",
    "",
    "\t// Configure the process object.",
    "\t// See functions' man pages for details.",
    "\t// Note that one must be root to change to root dir.",
    "\tif (getuid() == 0) {",
    "\t\tif (!process_set_rootdir(proc, rootdir))",
    "\t\t\tdie(\"Could not change root directory\\n\");",
    "",
    "\t\tif (!process_set_username(proc, user))",
    "\t\t\tdie(\"Could not set user name\\n\");",
    "\t}",
    "",
    "\t/* Configure some server values. Not needed, but makes it",
    "\t * to change values later. */",
    "\thttp_server_set_worker_threads(s, 8);",
    "\thttp_server_set_queue_size(s, 10);",
    "\thttp_server_set_max_pages(s, 20);",
    "",
    "\t/* Allocate all buffers needed */",
    "\tif(!http_server_alloc(s)) {",
    "\t\thttp_server_free(s);",
    "\t\texit(EXIT_FAILURE);",
    "\t}",
    "",
    "\t/* Add pages to the server */",
    };

    parr(f, mainfunc, sizeof mainfunc / sizeof *mainfunc);

    for (i = 0; i < argc; i++) {
        p(f, "\tif (!http_server_add_page(s, \"/%s\", %s, NULL))\n", argv[i], function_name(argv[i]));
        p(f, "\t\tdie(\"Could not add page to http server.\\n\");\n");
    }

    p(f, "\n");
    p(f, "\tif (!http_server_set_host(s, hostname))\n");
    p(f, "\t\tdie(\"Out of memory.\\n\");\n");
    p(f, "\n");
    p(f, "\thttp_server_set_port(s, portnumber);\n");
    p(f, "\n");
    p(f, "\n");
    p(f, "#if 0\n");
    p(f, "\thttp_server_set_documentroot(s, \"/path/to/my/root\");\n");
    p(f, "\thttp_server_set_can_read_files(s, 1);\n");
    p(f, "\thttp_server_set_post_limit(s, 1024 * 1024);\n");
    p(f, "#endif\n");
    p(f, "\n");

    p(f, "\t/* Start the server from the process object. */\n");
    p(f, "\tif (!http_server_start_via_process(proc, s))\n");
    p(f, "\t\tdie(\"Could not add http server to process object.\\n\");\n");
    p(f, "\n");

    p(f, "\tif (!process_start(proc, fork_and_close))\n");
    p(f, "\t\tdie(\"process_start failed: %%s\\n\", strerror(errno));\n");
    p(f, "\n");
    p(f, "\tif (!process_wait_for_shutdown(proc))\n");
    p(f, "\t\tdie(\"Failed to wait for shutdown.\\n\");\n");
    p(f, "\n");

    p(f, "\t/* Do general cleanup */\n");
    p(f, "\thttp_server_free(s);\n");
    p(f, "\tprocess_free(proc);\n");
    p(f, "\treturn 0;\n");
    p(f, "}\n");
    p(f, "\n");


    fclose(f);
}

static void create_makefile_am(int argc, char *argv[])
{
    FILE *f;
    int i;

    (void)argc;
    (void)argv;

    if ( (f = fopen("Makefile.am", "w")) == NULL) {
        perror("Makefile.am");
        exit(EXIT_FAILURE);
    }

    p(f, "bin_PROGRAMS=foo\n");
    p(f, "foo_SOURCES=");
    if (g_mainfile != NULL)
        p(f, "%s ", g_mainfile);

    for (i = 0; i < argc; i++)
        p(f, "%s ", argv[i]);

    p(f, "\n");
    p(f, "\n");


    /* No dist files */
    p(f, "nodist_foo_SOURCES=%s ", g_headerfile);
    for (i = 0; i < argc; i++)
        p(f, "%s.c ", remove_ext(argv[i]));
    p(f, "\n");
    p(f, "foo_CFLAGS=-W -Wall -pedantic -Wextra -std=gnu99 -Wshadow -Wmissing-prototypes -pthread\n");
    p(f, "foo_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread\n");
    p(f, "\n");

    /* Add all the translation rules for extensions? */
    p(f, "%%.c : %%.html\n");
    p(f, "\thipp -i %s -o $*.c $<\n", g_headerfile);
    p(f, "\n");

    /* Create the header file with the function prototypes */
    p(f, "%s: ", g_headerfile);
    for (i = 0; i < argc; i++)
        p(f, "%s ", argv[i]);
    p(f, "\n");
    p(f, "\thipp -pi $@ $+\n");
    p(f, "\n");


    p(f, "BUILT_SOURCES=$(nodist_foo_SOURCES)\n");
    p(f, "CLEANFILES=$(nodist_foo_SOURCES)\n");
    p(f, "\n");

    fclose(f);
}

static void create_configure_ac(int argc, char *argv[])
{
    FILE *f;
    (void)argc;
    (void)argv;

    if ( (f = fopen("configure.ac", "w")) == NULL) {
        perror("configure.ac");
        exit(EXIT_FAILURE);
    }

    p(f, "# Simple skeleton file, generated by hipp\n");
    p(f, "AC_PREREQ(2.57)\n");
    p(f, "AC_INIT(foo, 0.0.1, root@localhost)\n");
    p(f, "AM_INIT_AUTOMAKE\n");
    p(f, "AC_CONFIG_SRCDIR([%s])\n", g_mainfile ? g_mainfile : "foo.c");
    p(f, "\n");
    p(f, "# Checks for programs.\n");
    p(f, "AC_PROG_CC\n");
    p(f, "\n");
    p(f, "# Checks for libraries.\n");
    p(f, "AC_CHECK_LIB([highlander], [cstring_new])\n");
    p(f, "AC_CHECK_LIB([pthread], [pthread_create])\n");
    p(f, "\n");
    p(f, "AC_CONFIG_FILES([Makefile])\n");
    p(f, "AC_OUTPUT\n");
    p(f, "\n");

    fclose(f);
}

static void touch(const char *filename)
{
    int fd, flags = O_RDONLY | O_CREAT;

    fd = open(filename, flags, 0644);
    if (fd != -1)
        close(fd);
}


static void create_autoxx_files(int argc, char *argv[])
{
    create_makefile_am(argc, argv);
    create_configure_ac(argc, argv);
    touch("README");
    touch("AUTHORS");
    touch("NEWS");
    touch("ChangeLog");
}
