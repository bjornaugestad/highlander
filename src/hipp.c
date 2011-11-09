/**
 * hipp - The highlander preprocessor.
 * hipp converts HTML files to C files and supports embedded scripting
 * of C code. It is a simple program and works approximately like this:
 * - hipp looks for the tags <% and %>.
 * - everything OUTSIDE these tags are converted to a const char*[]
 *   with one entry per line.
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
 * Author : bjorn.augestad@gmail.com
 */

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring.h>
#include <assert.h>

#include <highlander.h>

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

int main(int argc, char *argv[])
{
	int i, c; 
	extern int optind;
	extern char* optarg;

	while( (c = getopt(argc, argv, "Am:t:o:i:Eshp")) != EOF) {
		switch(c) {
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

	if(g_headerfile == NULL) {
		fprintf(stderr, "%s: Required argument -i is missing.\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(optind == argc) {
		fprintf(stderr, "hipp: No input files\n");
		exit(EXIT_FAILURE);
	}
	else if(g_outputfile != NULL && optind + 1 < argc) {
		fprintf(stderr, "-o option is only valid if input is one file\n");
		exit(EXIT_FAILURE);
	}

	if(prototype_mode) 
		create_header(g_headerfile, argc, argv, optind);
	else {
		/* Generate C code for all the files */
		for(i = optind; i < argc; i++) 
			process_file(argv[i]);
	}


	/* 
	 * New stuff 20070401:
	 * -m can be used to generate a skeleton main function
	 * which sets up a web server for us.
	 */
	if(g_mainfile != NULL) 
		create_mainfile(argc-optind, &argv[optind], g_mainfile);

	if(m_automake)
		create_autoxx_files(argc-optind, &argv[optind]);


	exit(EXIT_SUCCESS);
}

static void show_help(void)
{
	printf("Please see the man page for detailed help\n");
	printf("\n");
}

static const char* legal_name(const char* base)
{
	char *s;
	static char name[1024];

	/* Create legal name */
	strcpy(name, base);
	s = name;
	while(*s != '\0') {
		if(!isalnum(*s) && *s != '_')
			*s = '_';

		s++;
	}

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
		"#include <stdio.h>\n"
		"#include <stdlib.h>\n"
		"#include <unistd.h>\n"
		"#include <string.h>\n"
		"#include <assert.h>\n"
		"\n"
		"#include <highlander.h>\n"
		;


	fprintf(f, "%s\n", headers);
	fprintf(f, "#include \"%s\"\n", g_headerfile);
	fprintf(f, "\n");

}

static void print_fn(FILE*f, char* name)
{
	const char* fn = function_name(name);

	fprintf(f, "int %s(http_request request, http_response response)\n", fn);
	fprintf(f, "{\n");
	fprintf(f, "\tassert(request != NULL);\n");
	fprintf(f, "\tassert(response != NULL);\n");
	fprintf(f, "\n");

	if(g_content_type != NULL) {
		fprintf(f, "\tresponse_set_content_type(response, \"%s\");\n", g_content_type);
		fprintf(f, "\n");
	}
}

static void write_html_buffer(FILE* f, const char* str)
{
	int printed = 0;
	const char *s;
	char last = '\0';
	const char* wrappers = "> ";

	s = str;
	fprintf(f, "\t{\n");
	fprintf(f, "\t\tconst char* html = \n");
	fprintf(f, "\t\t\"");
	while(*s) {
		if(isspace(*s) && m_strip_blanks && printed == 0) {
			s++;
			continue;
		}
		else if(*s == '\\') {
			fprintf(f, "\\\\");
		}
		else if(*s == '\n') {
			if(m_strip_blanks) {
				fprintf(f, "\\n");
			}
		}
		else if(*s == '"' && last != '\\')
			fprintf(f, "\\%c", *s);
		else
			fputc(*s, f);


		printed++;
		if(*s == '\n'
		|| (last != '\\' && strchr(wrappers, *s) && printed > 70)) {
			printed = 0;
			fprintf(f, "\\n\"\n\t\t\"");
			last = '\0';
		}
		else 
			last = *s;

		s++;
	}

	fprintf(f, "\";\n");
	fprintf(f, "\t\tresponse_add(response, html);\n");
	fprintf(f, "\t}\n");
}

static const char* remove_ext(char* name)
{
	static char buf[10000];
	char *s;

	strcpy(buf, name);
	s = buf + strlen(buf) - 1;
	while(s != buf) {
		if(*s == '.') {
			*s = '\0';
			break;
		}
		s--;
	}

	if(s == buf) {
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

static void process_file(char* name)
{
	FILE *fin, *fout;
	char fname[10240];
	char base[10240];
	int in_header = 0, in_hipp = 0;
	int c;
	int fn_written = 0;


	/* buffers html */
	cstring buf = cstring_new();

	if(!get_basename(name, NULL, base, sizeof base)) {
		perror(name);
		exit(EXIT_FAILURE);
	}

	if(g_outputfile != NULL)
		strcpy(fname, g_outputfile);
	else {
		strcpy(fname, remove_ext(base));
		strcat(fname, ".c");
	}

	lineno = 1;
	g_current_file = name;
	if( (fin = fopen(name, "r")) == NULL) {
		perror(name);
		exit(EXIT_FAILURE);
	}
	else if( (fout = fopen(fname, "w")) == NULL) {
		perror(fname);
		exit(EXIT_FAILURE);
	}

	print_standard_stuff(fout);
	print_line_directive(fout, g_current_file, lineno);

	while( (c = fgetc(fin)) != EOF) {
		switch(c) {
			case '<':
				if(peek(fin) == '%') {
					if(in_hipp || in_header) {
						fprintf(stderr, "hipp: Tags do not nest\n");
						exit(EXIT_FAILURE);
					}

					/* We're entering hipp mode, print buf */
					in_hipp = 1;
					if(!fn_written) {
						print_fn(fout, name);
						fn_written = 1;
					}

					write_html_buffer(fout, c_str(buf));
					cstring_recycle(buf);
					print_line_directive(fout, g_current_file, lineno);
					fgetc(fin); /* Skip % */
				}
				else
					cstring_charcat(buf, c);

				break;

			case '%':
				if(peek(fin) == '>') {
					/* We're leaving hipp code. Print it */
					in_hipp = 0;
					fprintf(fout, "%s", c_str(buf));
					cstring_recycle(buf);
					fgetc(fin); /* Skip > */
				}
				else if (peek(fin) == '{') {
					if(in_hipp || in_header) {
						fprintf(stderr, "hipp: Tags do not nest\n");
						exit(EXIT_FAILURE);
					}

					/* We're entering header mode */
					in_header = 1;
					cstring_recycle(buf);
					fgetc(fin); /* Skip { */
				}
				else if (peek(fin) == '}') {
					/* We're leaving header mode */
					in_header = 0;
					fprintf(fout, "%s", c_str(buf));
					cstring_recycle(buf);
					fgetc(fin); /* Skip } */
				}
				else {
					cstring_charcat(buf, '%');
				}

				break;

			default:
				if(c == '\n')
					lineno++;

				/* Add character to buffer */
				cstring_charcat(buf, c);
				break;
		}
	}

	if(!fn_written) 
		print_fn(fout, name);

	if(cstring_length(buf) > 0) 
		write_html_buffer(fout, c_str(buf));

	fprintf(fout, "\treturn 0;\n");
	fprintf(fout, "}\n");


	fclose(fin);
	fclose(fout);
}

static void print_line_directive(FILE *f, const char* file, int line)
{
	if(!m_skip_line_numbers) 
		fprintf(f, "#line %d \"%s\"\n", line, file);
}

static void create_header(char* filename, int argc, char *argv[], int opt_ind)
{
	int i;
	FILE* f;
	char base[10240], *s, guard[2048];

	get_basename(filename, NULL, base, sizeof base);
	strcpy(guard, remove_ext(base));
	s = guard;
	while(*s != '\0')  {
		*s = toupper((unsigned char)*s);
		s++;
	}

	if( (f = fopen(filename, "w")) == NULL) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	fprintf(f, "#ifndef HIPP_%s_H\n", guard);
	fprintf(f, "#define HIPP_%s_H\n", guard);

	fprintf(f, "\n");
	fprintf(f, "#include <highlander.h>\n");
	fprintf(f, "\n");

	fprintf(f, "\n");
	fprintf(f, "#ifdef __cplusplus\n");
	fprintf(f, "extern \"C\" {\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");


	for(i = opt_ind; i < argc; i++) 
		fprintf(f, "int %s(http_request request, http_response response);\n", function_name(argv[i]));

	fprintf(f, "\n");
	fprintf(f, "#ifdef __cplusplus\n");
	fprintf(f, "}\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");
	fprintf(f, "#endif /* guard */\n");

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

	if( (f = fopen(filename, "w")) == NULL)
		die_perror("%s", filename);

	/* Include all the stock include files */
	fprintf(f, "#include <stdlib.h>\n");
	fprintf(f, "#include <highlander.h>\n");
	fprintf(f, "\n");

	/* Include the header file which we use */
	fprintf(f, "#include \"%s\"\n", g_headerfile);
	fprintf(f, "\n");

	fprintf(f, "int main(int argc, char *argv[])\n");
	fprintf(f, "{\n");
	fprintf(f, "	http_server s;\n");
	fprintf(f, "\n");
	fprintf(f, "	/* Silence the compiler */\n");
	fprintf(f, "	(void)argc;\n");
	fprintf(f, "	(void)argv;\n");
	fprintf(f, "\n");
	fprintf(f, "	/* First we create the web server */\n");
	fprintf(f, "	if( (s = http_server_new()) == NULL)\n");
	fprintf(f, "		exit(EXIT_FAILURE);\n");
	fprintf(f, "\n");
	fprintf(f, "	/* Then configure the memory requirements */\n");
	fprintf(f, "	/* Here are some dummy statements to make it easier for the user */\n");

	fprintf(f, "#if 1\n");
	fprintf(f, "	http_server_set_worker_threads(s, 8);\n");
	fprintf(f, "	http_server_set_queue_size(s, 10);\n");
	fprintf(f, "	http_server_set_max_pages(s, 20);\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");
	fprintf(f, "\n");
	fprintf(f, "	/* Allocate all buffers needed */\n");
	fprintf(f, "	if(!http_server_alloc(s)) {\n");
	fprintf(f, "		http_server_free(s);\n");
	fprintf(f, "		exit(EXIT_FAILURE);\n");
	fprintf(f, "	}\n");
	fprintf(f, "\n");
	fprintf(f, "	/* Add pages to the server */\n");

	for(i = 0; i < argc; i++) {
		fprintf(f, "	http_server_add_page(s, \"/%s\", %s, NULL);\n", argv[i], function_name(argv[i]));
	}

	fprintf(f, "	/* More configuration settings */\n");
	fprintf(f, "#if 1\n");
	fprintf(f, "	http_server_set_timeout_read(s, 5);\n");
	fprintf(f, "	http_server_set_timeout_write(s, 5);\n");
	fprintf(f, "	http_server_set_timeout_accept(s, 5);\n");
	fprintf(f, "	http_server_set_retries_read(s, 0);\n");
	fprintf(f, "	http_server_set_retries_write(s, 2);\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");
	fprintf(f, "\n");
	fprintf(f, "#if 1\n");
	fprintf(f, "	http_server_set_block_when_full(s, 0);\n");
	fprintf(f, "	http_server_set_logfile(s, \"my_logfile\");\n");
	fprintf(f, "	http_server_set_logrotate(s, 100000);\n");
	fprintf(f, "\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");
	fprintf(f, "#if 1\n");
	fprintf(f, "	http_server_set_host(s, \"localhost\");\n");
	fprintf(f, "	http_server_set_port(s, 2000); /* Good while testing */\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");
	fprintf(f, "#if 1\n");
	fprintf(f, "	http_server_set_documentroot(s, \"/path/to/my/root\");\n");
	fprintf(f, "	http_server_set_can_read_files(s, 0);\n");
	fprintf(f, "	http_server_set_post_limit(s, 1024*1024);\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");

	fprintf(f, "	/* Allocate root resources (ie bind to the port) */\n");
	fprintf(f, "	if(!http_server_get_root_resources(s)) {\n");
	fprintf(f, "		http_server_free(s);\n");
	fprintf(f, "		exit(EXIT_FAILURE);\n");
	fprintf(f, "	}\n");
	fprintf(f, "\n");
	fprintf(f, "	/* Start the server */\n");
	fprintf(f, "	http_server_start(s);\n");
	fprintf(f, "\n");
	fprintf(f, "	/* Do general cleanup */\n");
	fprintf(f, "	http_server_free(s);\n");
	fprintf(f, "	return 0;\n");
	fprintf(f, "}\n");
	fprintf(f, "\n");


	fclose(f);
}

static void create_makefile_am(int argc, char *argv[])
{
	FILE *f;
	int i;

	(void)argc;
	(void)argv;

	if( (f = fopen("Makefile.am", "w")) == NULL)
		die_perror("Makefile.am");

	fprintf(f, "bin_PROGRAMS=foo\n");
	fprintf(f, "foo_SOURCES=");
	if(g_mainfile != NULL)
		fprintf(f, "%s ", g_mainfile);

	for(i = 0; i < argc; i++) 
		fprintf(f, "%s ", argv[i]);
	
	fprintf(f, "\n");
	fprintf(f, "\n");


	/* No dist files */
	fprintf(f, "nodist_foo_SOURCES=%s ", g_headerfile);
	for(i = 0; i < argc; i++) 
		fprintf(f, "%s.c ", remove_ext(argv[i]));
	fprintf(f, "\n");
	fprintf(f, "foo_CFLAGS=-W -Wall -pedantic -Wshadow -Wmissing-prototypes -Winline -Wno-long-long -pthread\n");
	fprintf(f, "\n");

	/* Add all the translation rules for extensions? */
	fprintf(f, "%%.c : %%.html\n");
	fprintf(f, "	hipp -i %s -o $*.c $<\n", g_headerfile);
	fprintf(f, "\n");

	/* Create the header file with the function prototypes */
	fprintf(f, "%s: ", g_headerfile);
	for(i = 0; i < argc; i++)
		fprintf(f, "%s ", argv[i]);
	fprintf(f, "\n");
	fprintf(f, "	hipp -pi $@ $+\n");
	fprintf(f, "\n");


	fprintf(f, "BUILT_SOURCES=$(nodist_foo_SOURCES)\n");
	fprintf(f, "CLEANFILES=$(nodist_foo_SOURCES)\n");
	fprintf(f, "\n");

	fclose(f);
}

static void create_configure_ac(int argc, char *argv[])
{
	FILE *f;
	(void)argc;
	(void)argv;

	if( (f = fopen("configure.ac", "w")) == NULL)
		die_perror("configure.ac");

	fprintf(f, "# Simple skeleton file, generated by hipp\n");
	fprintf(f, "AC_PREREQ(2.57)\n");
	fprintf(f, "AC_INIT(foo, 0.0.1, root@localhost)\n");
	fprintf(f, "AM_INIT_AUTOMAKE\n");
	fprintf(f, "AC_CONFIG_SRCDIR([%s])\n", g_mainfile ? g_mainfile : "foo.c");
	fprintf(f, "\n");
	fprintf(f, "# Checks for programs.\n");
	fprintf(f, "AC_PROG_CC\n");
	fprintf(f, "\n");
	fprintf(f, "# Checks for libraries.\n");
	fprintf(f, "AC_CHECK_LIB([highlander], [cstring_new])\n");
	fprintf(f, "AC_CHECK_LIB([pthread], [pthread_create])\n");
	fprintf(f, "\n");
	fprintf(f, "AC_CONFIG_FILES([Makefile])\n");
	fprintf(f, "AC_OUTPUT\n");
	fprintf(f, "\n");

	fclose(f);
}

static void create_autoxx_files(int argc, char *argv[])
{
	create_makefile_am(argc, argv);
	create_configure_ac(argc, argv);
}
