#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <pthread.h>

#include <cstring.h>
#include <meta_html.h>
#include <meta_list.h>

/* A section is stored using this structure */
typedef struct section_tag {
	char* name;
	int width;
	int height;
	char* code;
}* section;

struct html_template_tag {
	int width;
	int height;
	char* layout;
	list sections;
	html_menu menu;
	pthread_mutex_t menulock;
	cstring rendered_menu;
};

html_template html_template_new(void)
{
	html_template t;

	if( (t = malloc(sizeof *t)) == NULL)
		;
	else if( (t->sections = list_new()) == NULL) {
		free(t);
		t = NULL;
	}
	else {
		t->width = 0;
		t->height = 0;
		t->layout = NULL;
		t->menu = NULL;
		t->rendered_menu = NULL;
		pthread_mutex_init(&t->menulock, NULL);
	}

	return t;
}

void html_template_set_menu(html_template t, html_menu m)
{
	if(t->menu != NULL)
		html_menu_free(t->menu);

	t->menu = m;
}


void html_template_free(html_template t)
{
	if(t != NULL) {
		list_free(t->sections, (void(*)(void*))html_section_free);
		free(t->layout);
		html_menu_free(t->menu);
		pthread_mutex_destroy(&t->menulock);
		cstring_free(t->rendered_menu);
		free(t);
	}
}

int html_template_set_layout(html_template t, const char* s)
{
	assert(t != NULL);
	assert(s != NULL);

	if(t->layout != NULL)
		free(t->layout);

	if( (t->layout = malloc(strlen(s) + 1)) != NULL)
		strcpy(t->layout, s);

	return t->layout != NULL;
}

int html_template_add_section(html_template t, html_section s)
{
	assert(t != NULL);
	assert(s != NULL);

	return list_add(t->sections, s) != NULL ? 1 : 0;
}

int html_template_add_user_section(html_template t)
{
	html_section s;

	if( (s = html_section_new()) == NULL)
		return 0;
	else if(html_section_set_name(s, "user") == 0) {
		html_section_free(s);
		return 0;
	}
	else if(list_add(t->sections, s) == NULL) {
		html_section_free(s);
		return 0;
	}
	else
		return 1;
}

/**
 * Here we create the html page based on the template layout.
 */

int html_template_send(
	html_template t,
	http_response response,
	const char* headcode,
	const char* usercode)
{
	html_section sect;
	size_t i = 0;
	const char *identifier, *s, *sectname;

	assert(t != NULL);
	assert(response != NULL);
	assert(usercode != NULL);
	assert(t->layout != NULL);

	/* Render the menu once to avoid too much rt overhead */
	if(t->menu != NULL) {
		pthread_mutex_lock(&t->menulock);
		if(t->rendered_menu != NULL) {
			pthread_mutex_unlock(&t->menulock);
		}
		else if( (t->rendered_menu = cstring_new()) == NULL) {
			pthread_mutex_unlock(&t->menulock);
			return 0;
		}
		else if(!html_menu_render(t->menu, t->rendered_menu)) {
			/* Error rendering menu */
			cstring_free(t->rendered_menu);
			t->rendered_menu = NULL;
			pthread_mutex_unlock(&t->menulock);
			return 0;
		}
		else {
			/* Menu rendered ok. Unlock and continue */
			pthread_mutex_unlock(&t->menulock);
		}
	}

	s = t->layout;

	i = 0;
	while( (identifier = strchr(s, '%')) != NULL) {
		/* Print everything from s to (identifier - 1) */
		if(!response_add_end(response, s, identifier))
			return 0;

		identifier++; /* Skip the % */
		
		switch(*identifier) {
			case '\0':
				break;

			case 'S':
				if( (sect = list_get_item(t->sections, i++)) == NULL)
					return 0;

				sectname = html_section_get_name(sect);
				if(strcmp(sectname, "user") == 0)
					response_add(response, usercode);
				else
					response_add(response, html_section_get_code(sect));
				break;

			case 'H':
				response_add(response, headcode);
				break;

			case 'M':
				if(t->menu != NULL) {
					if(!response_add(response, c_str(t->rendered_menu))) {
						return 0;
					}
				}
				break;

			default:
				/* Just add the unknown character  as well as the % */
				response_add_char(response, '%');
				response_add_char(response, *identifier);
				break;
		}

		/* was % found as last character */
		if(identifier != '\0')
			identifier++;

		s = identifier;
	}

	/* Add the rest of the string */
	response_add(response, s);
	return 1;
}

