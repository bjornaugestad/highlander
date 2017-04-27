/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */



/**
 * bin2c - Create a C representation of something binary.
 * The input is normally an image, and the output is always 
 * a C source file and/or a C header file. The header file contains a function
 * declaration.
 *
 * The dynamic function will be named show_XXX where XXX is the name
 * of the parameter. foo.jpeg becomes show_foo_jpeg.
 *
 * The name of the array is foo_jpeg. The array will be static in the c file.
 * Multiple files can be processed in one operation. This requires us
 * to name the c and h files on the command line.
 */


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>

void process(const char* filename, FILE *source, FILE *header);
void create_arrays(const char* filename, FILE *source);
void create_implementations(const char* filename, FILE *source);
void create_declarations(const char* filename, FILE *f);
void create_calls(const char* filename);
void show_usage(void);
const char *base(const char *s);
void start_source(FILE *f, const char *headerfile);
void start_header(FILE *f, const char *filename);
void end_source(FILE *f);
void end_header(FILE *f);

const char *g_content_type = NULL;
int g_verbose = 0;
int g_veryverbose = 0;
int g_store_as_text = 0;
int g_create_access_function = 0;

/* Use only the basename of the input file for the name of the 
 * generated function. Needed when we build and the source resides
 * in a different directory. (Try make distcheck without -b to see..)
 */
int g_basename_only = 0;


int main(int ac, char** av)
{
	extern char *optarg;
	int i, c;
	const char *sourcefile = NULL, *headerfile = NULL, *includefile = NULL;
	FILE *source, *header;

	while( (c = getopt(ac, av, "bai:TvVt:c:h:")) != EOF) {
		switch(c) {
			case 'a':
				g_create_access_function = 1;
				break;
			
			case 'b':
				g_basename_only = 1;
			case 'c':
				sourcefile = optarg;
				break;
			
			case 'h':
				headerfile = optarg;
				break;

			case 'i':
				includefile = optarg;
				break;

			case 't':
				g_content_type = optarg;
				break;

			case 'T':
				g_store_as_text = 1;
				break;

			case 'v':
				g_verbose = 1;
				break;

			case 'V':
				g_veryverbose = 1;
				break;

			default:
				show_usage();
				break;
		}
	}

	/* If no filenames were provided */
	if(optind == ac) {
		show_usage();
		exit(EXIT_FAILURE);
	}

	/*
	 * Now for some semantics. We can generate the header file without 
	 * specifying the type, but we need the type if we generate the source file.
	 * bin2c can generate just the header, just the source file, or both header 
	 * and source. It is meaningless to generate nothing...
	 */
	if(sourcefile == NULL && headerfile == NULL) {
		fprintf(stderr, "You should generate something.\n");
		show_usage();
		exit(EXIT_FAILURE);
	}

	/* We need content type if we generate source */
	if(sourcefile != NULL && g_content_type == NULL) {
		fprintf(stderr, "Please specify content type\n");
		show_usage();
		exit(EXIT_FAILURE);
	}

	/* We need -i includefile if -h header isn't provided and we generate source .
	 * It is confusing to provide both -i and -h, so we disallow that. -h wins. 
	 */
	if(sourcefile != NULL) {
		if((headerfile == NULL && includefile == NULL)
		|| (headerfile != NULL && includefile != NULL)) {
			fprintf(stderr, "Please provide either -i includefile or -h headerfile\n");
			exit(EXIT_FAILURE);
		}

		/*
		 * The test above should assert that one and only 
		 * one file is provided. 
		 */
		if (headerfile != NULL)
			includefile = headerfile;
	}

	source = NULL;
	if(sourcefile != NULL && (source = fopen(sourcefile, "w")) == NULL) {
		perror(sourcefile);
		exit(EXIT_FAILURE);
	}

	header = NULL;
	if(headerfile != NULL && (header = fopen(headerfile, "w")) == NULL) {
		perror(headerfile);
		exit(EXIT_FAILURE);
	}

	if(source != NULL) {
		start_source(source, includefile);
		for(i = optind; i < ac; i++)
			create_arrays(av[i], source);

		for(i = optind; i < ac; i++)
			create_implementations(av[i], source);

		end_source(source);
		fclose(source);
	}

	if(header != NULL) {
		start_header(header, headerfile);
	
		for(i = optind; i < ac; i++)
			create_declarations(av[i], header);

		end_header(header);
		fclose(header);
	}

	if(g_veryverbose) {
		for(i = optind; i < ac; i++)
			create_calls(av[i]);
	}

	return 0;
}

void create_bin_arrays(const char* filename, FILE *f);
void create_text_arrays(const char* filename, FILE *f);
void create_arrays(const char* filename, FILE *f) 
{
	if(g_store_as_text) 
		create_text_arrays(filename, f);
	else
		create_bin_arrays(filename, f);
}

static void remove_trailing_newline(char *sz)
{
	size_t n = strlen(sz);
	while(n-- > 0 && isspace(sz[n]))
			sz[n] = '\0';
}

void create_text_arrays(const char* filename, FILE *f) 
{
	char *s, sz[20480];
	FILE *fin;

	if( (fin = fopen(filename, "r")) == NULL) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	fprintf(f, "\nstatic const unsigned char x%s[] = \n", base(filename));
	while(fgets(sz, sizeof sz, fin)) {
		remove_trailing_newline(sz);
		s = sz;
		fputc('"', f);
		while(*s) {
			if(*s == '"')
				fprintf(f, "\\%c", *s);
			else
				fputc(*s, f);
			s++;
		}
		fprintf(f, "\\n\"\n");
	}

	fclose(fin);
	fprintf(f, "\n;\n");
}

void create_bin_arrays(const char* filename, FILE *f) 
{
	int fd, printed;
	size_t cb;
	unsigned char line[2048];

	if( (fd = open(filename, O_RDONLY)) == -1) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	fprintf(f, "\nstatic const unsigned char x%s[] = {\n", base(filename));

	printed = 0;
	while( (cb = read(fd, line, sizeof line)) > 0) {
		size_t i;
		for(i = 0; i < cb; i++) {
			printed += fprintf(f, "%d,", line[i]);

			if(printed > 72) {
				fprintf(f, "\n");
				printed = 0;
			}
		}
	}

	fprintf(f, "\n};\n\n");
	close(fd);
}


void show_usage(void)
{
static const char *usage1 =
"USAGE: bin2c [-bvVT] -c sourcefile -h headerfile -t content_type binary_file...\n"
"Creates C source files suitable for Highlander from other files.\n"
"bin2c is a tool used to embed e.g. images in C source.\n"
"\t-t content-type is e.g. image/jpeg, image/png\n"
"\t-v verbose \n"
"\t-i includefile. The generated C file will include this file.\n"
"\t-V is very verbose. -V will generate extra code to add dynamic pages\n"
"\t-T Store data as text. Good for html files\n"
"\t-b Basename. Use only the basename of the binary file for function name, strip directory path\n"
;

static const char *usage2 =
"\t-c source file. Store generated code in this file.\n"
"\t-h header file. Declares the generated functions.\n"
"\t-a Access function. Creates an access function which returns a pointer\n"
"\t   to the embedded data. Note that we can only create access functions\n"
"\t   when the data is stored as text. Text is zero-terminated.\n"
"\t   Future versions may include a function which returns the size\n"
"\t   of the embedded object.\n"
;

	printf("%s%s\n", usage1, usage2);
}


const char *base(const char *s)
{
	static char bf[2048];
	size_t i;

	if(g_basename_only) {
		/* Copy only the filename to the buffer bf. */
		const char* end = s + strlen(s);
		while(end > s && *end != '/')
			end--;

		if(*end == '/')
			end++;

		strcpy(bf, end);
	}
	else {
		strcpy(bf, s);
	}

	i = 0;
	while(bf[i] != '\0') {
		if(!isalnum(bf[i]))
			bf[i] = '_';
		i++;
	}

	return bf;
}

void create_implementations(const char* filename, FILE *f)
{
	const char *name = base(filename);

	if(g_create_access_function && g_store_as_text) {
		fprintf(f, "const unsigned char* get_x%s(void)\n", name);
		fprintf(f, "{\n");
		fprintf(f, "\treturn x%s;\n", name);
		fprintf(f, "}\n");
	}

	fprintf(f, "int show_%s(http_request request, http_response response)\n",
		name);
	fprintf(f, "{\n");
	fprintf(f, "\t(void)request;\n");
	fprintf(f, "\tif (!response_set_content_type(response, \"%s\"))\n", g_content_type);
	fprintf(f, "\t\treturn HTTP_500_INTERNAL_SERVER_ERROR;\n");
	fprintf(f, "\n");

	fprintf(f, "\tresponse_set_content_buffer(response, (void*)x%s, sizeof(x%s));\n",
		name, name);
	fprintf(f,"\treturn 0;\n");
	fprintf(f, "}\n\n");
}

void start_source(FILE *f, const char *headerfile)
{
	fprintf(f, "#include <highlander.h>\n");
	fprintf(f, "#include \"%s\"\n", headerfile);
}

void start_header(FILE *f, const char *filename)
{
	char *s, guard[2048];

	strcpy(guard, filename);

	/* Create uppercase and replace [./] with _ */
	s = guard;
	while(*s) {
		if(islower(*s))
			*s = toupper(*s);
		else if (!isalnum(*s))
			*s = '_';
		
		s++;
	}

	fprintf(f, "#ifndef %s\n", guard);
	fprintf(f, "#define %s\n\n", guard);
	fprintf(f, "\n");
	
	fprintf(f, "#ifdef __cplusplus\n");
	fprintf(f, "extern \"C\" {\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");
}

void end_source(FILE *f)
{
	(void)f;
}

void end_header(FILE *f)
{
	fprintf(f, "\n");
	fprintf(f, "#ifdef __cplusplus\n");
	fprintf(f, "}\n");
	fprintf(f, "#endif\n");
	fprintf(f, "\n");

	fprintf(f, "\n#endif\n");
}

void create_declarations(const char* filename, FILE *f)
{
	const char *name = base(filename);

	if(g_create_access_function && g_store_as_text) {
		fprintf(f, "const unsigned char* get_x%s(void);\n", name);
	}

	fprintf(f, "int show_%s(http_request request, http_response response);\n",
		name);
}

void create_calls(const char* filename)
{
	printf("\thttp_server_add_page(s, \"/%s\", show_%s, NULL);\n",
		filename, base(filename));
}

