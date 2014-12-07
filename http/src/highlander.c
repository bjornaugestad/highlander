/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <assert.h>

#include <rfc1738.h>
#include <meta_misc.h>

#include "internals.h"

/* Local helper functions */
static int serviceConnection2(
	http_server srv,
	connection conn,
	http_request req,
	http_response response,
	meta_error e);

static int check_attributes(http_request request, page_attribute a)
{
	const char* page_val;

	assert(request != NULL);
	assert(a != NULL);

	/* See if the client understands us */
	page_val = attribute_get_media_type(a);
	if (page_val && !request_accepts_media_type(request, page_val))
		return 0;

	return 1;
}

/* Checks to see if incoming accept limits fits the page attributes */
static int fs_can_run(http_server srv, http_request request, dynamic_page p)
{
	page_attribute a;

	assert(srv != NULL);
	assert(request != NULL);
	assert(p != NULL);

	a = dynamic_get_attributes(p);
	if (NULL == a)
		a = http_server_get_default_attributes(srv);

	/* None set, run anything */
	if (NULL == a)
		return 1;

	return check_attributes(request, a);
}

/*
 * Here we send a disk file to the client, if the file exists.
 * A few rules:
 * a) The URI must be valid, no ..
 * b) The documentroot will be prepended to the file. No empty
 *	  documentroots allowed. Minimum docroot is 2 characters, and
 *	  it cannot contain "..". The docroot must either be ./ or / or
 *	  something longer.
 */
static int send_disk_file(
	http_server s,
	connection conn,
	http_request req,
	http_response response,
	meta_error e)
{
	size_t i;
	const char *uri, *docroot;
	char filename[CCH_URI_MAX + DOCUMENTROOT_MAX + 2];
	struct stat st;
	page_attribute a;
	http_version v;
	const char* content_type;

	assert(s != NULL);
	assert(conn != NULL);
	assert(req != NULL);
	assert(response != NULL);
	assert(e != NULL);

	(void)conn; /* TODO: Sjekk hvorfor funksjonen ikke sender fila??? */

	/* We need a valid uri */
	if ((uri = request_get_uri(req)) == NULL
	|| strlen(uri) == 0
	|| strstr(uri, ".."))
		return set_http_error(e, HTTP_400_BAD_REQUEST);

	/* We need a valid documentroot */
	if ((docroot = http_server_get_documentroot(s)) == NULL)
		return set_http_error(e, HTTP_400_BAD_REQUEST);

	i = strlen(docroot);
	if (i < 1
	|| (i == 1 && *docroot != '/')
	|| (i == 2 && strcmp(docroot, "./") != 0)
	|| (docroot[0] == '.' && docroot[1] == '.')
	|| strstr(docroot, "..") != NULL )
		return set_http_error(e, HTTP_400_BAD_REQUEST);

	/* We need space for the absolute path */
	i += strlen(uri) + 2; /* 2 is one for slash and one for \0 */
	if (i >= sizeof(filename))
		return set_http_error(e, HTTP_400_BAD_REQUEST);

	/* Create the path */
	sprintf(filename, "%s/%s", docroot, uri);

	/* Does the file exist? */
	if (stat(filename, &st))
		return set_http_error(e, HTTP_404_NOT_FOUND);

	if (S_ISREG(st.st_mode))
		;
	else if (S_ISDIR(st.st_mode)) {
		/* BUG? If docroot+uri+index.html > sizeof(filename) we have issues */
		strcat(filename, "/index.html");
		if (stat(filename, &st))
			return set_http_error(e, HTTP_404_NOT_FOUND);

		if (!S_ISREG(st.st_mode))
			return set_http_error(e, HTTP_400_BAD_REQUEST);
	}
	else
		return set_http_error(e, HTTP_400_BAD_REQUEST);

	/* We must check page_attributes even for files loaded from disk. */
	/* NOTE: Trengs dette for HTTP 1.0 ?*/

	v = request_get_version(req);
	(void)v;
	if ((a = http_server_get_default_attributes(s)) != NULL
	&& !check_attributes(req, a)) {
		response_set_status(response, HTTP_406_NOT_ACCEPTABLE);
		return set_http_error(e, HTTP_406_NOT_ACCEPTABLE);
	}

	content_type = get_mime_type(filename);

	/*
	 * This function does not actually send the file, just stats it
	 * and stores the path. The contents will be sent later when
	 * response_send_entity is called.
	 */
	if (!response_send_file(response, filename, content_type, e))
		return 0;

	response_set_status(response, HTTP_200_OK);
	return 1;
}

/* Call the callback function for the page */
int handle_dynamic(
	connection conn,
	http_server srv,
	dynamic_page p,
	http_request req,
	http_response response,
	meta_error e)
{
	int status = HTTP_200_OK;
	http_version version;

	assert(NULL != p);

	version = request_get_version(req);
	if (!fs_can_run(srv, req, p)) {
		response_set_status(response, HTTP_406_NOT_ACCEPTABLE);
		return set_http_error(e, HTTP_406_NOT_ACCEPTABLE);
	}

	/* NOTE/TODO: This seems to be a good place to add authorization stuff.
	 * Check if the request has authorization info and send 401 if not.
	 * We don't have to keep state as the next request will
	 * have all autorization stuff needed to keep going.
	 * boa 20080125.
	 * PS: This comment is added because Tandberg wants me to add support
	 * for RFC2617 HTTP Authentication
	 */
	request_set_connection(req, conn);
	response_set_version(response, version);
	response_set_last_modified(response, time(NULL));

	/*
	 * Run the dynamic function. It is supposed to return 0 for OK,
	 * but we accept any legal HTTP status code. Illegal status codes
	 * are mapped to 500.
	 */
	if ((status = dynamic_run(p, req, response)))
		status = http_status_code(status) ? status : HTTP_500_INTERNAL_SERVER_ERROR;
	else
		status = HTTP_200_OK;

	response_set_status(response, status);
	return 1;
}

/* uri params are separated by =, so if there are any = in the string,
 * we have more params */

#if 0
/*
 * Call this function if the request ended in some kind of HTTP error.
 * Typical errors are 404 not found, 400 bad request.
 * Do not call it for other errors.
 *
 * The function sends the proper HTTP headers back to the client
 * if possible. It also logs the error to the servers log file.
 */
static void handle_http_error(
	http_server srv,
	connection conn,
	http_request req,
	int error)
{
	http_version v;
	assert(http_status_code(error));

	v = request_get_version(req);
	send_status_code(conn, error, v);
	http_server_add_logentry(srv, conn, req, error, 0);
}

static int semantic_error(http_request request)
{
	/*
	 * Now for some semantic stuff.
	 * a) rfc2616 requires that all 1.1 messages includes a Host:
	 * and that servers MUST respond with 400 if Host: is missing.
	 */
	http_version v = request_get_version(request);
	if (v == VERSION_11 && request_get_host(request) == NULL)
		return 1;

	/* OK */
	return 0;
}
#endif

void* serviceConnection(void* psa)
{
	int success;
	connection conn;
	http_server srv;
	http_request request;
	http_response response;
	meta_error e = meta_error_new();

	conn = psa;
	srv =  connection_arg2(conn);
	request = http_server_get_request(srv);
	request_set_defered_read(request, http_server_get_defered_read(srv));
	response = http_server_get_response(srv);

	success = serviceConnection2(srv, conn, request, response, e);
	if (!success && is_tcpip_error(e))
		connection_discard(conn);
	else
		connection_close(conn);

	http_server_recycle_request(srv, request);
	http_server_recycle_response(srv, response);

	meta_error_free(e);
	return (void*)(intptr_t)success;
}

static int serviceConnection2(
	http_server srv,
	connection conn,
	http_request request,
	http_response response,
	meta_error e)
{
	dynamic_page dp;
	size_t cbSent;
	int status, error = 0;
	size_t max_posted_content = http_server_get_post_limit(srv);

	while (!http_server_shutting_down(srv)) {
		if (!data_on_socket(conn))
			return set_tcpip_error(e, EAGAIN);

		/* Were we able to read a valid http request?
		 * If not, what is the cause of the error? If it is a http
		 * protocol error, we try to send a response back to the client
		 * and close the connection. If it is anything else(tcp/ip, os)
		 * we stop processing.
		 */
		error = !request_receive(request, conn, max_posted_content, e);

		/* So far, so good. We have a valid HTTP request.
		 * Now see if we can locate a page handler function for it.
		 * If we do, call it. If not, see if it on disk or if the
		 * http_server has a default page handler. If neither is true,
		 * then the page was not found(404).
		 */
		if (error)
			;
		else if ((dp = http_server_lookup(srv, request)) != NULL) {
			if (!handle_dynamic(conn, srv, dp, request, response, e)) {
				error = 1;
			}
		}
		else if (http_server_can_read_files(srv)) {
			if (!send_disk_file(srv, conn, request, response, e)) {
				error = 1;
			}
		}
		else if (http_server_has_default_page_handler(srv)) {
			if (!http_server_run_default_page_handler(conn, srv, request, response, e)) {
				error = 1;
			}
		}
		else {
			/* We didn't find the page */
			response_set_status(response, HTTP_404_NOT_FOUND);
			response_set_connection(response, "close");
		}

		if (error) {
			if (is_protocol_error(e)) {
				status = get_error_code(e);
				response_set_status(response, status);
				response_set_connection(response, "close");
				cbSent = response_send(response, conn, e);
				http_server_add_logentry(srv, conn, request, status, cbSent);
			}

			return 0;
		}

		/*
		 * Some extra stuff for HTTP 1.0 clients. If client is 1.0
		 * and connection_close() == 1 and connection header field
		 * isn't set, then we set the connection flag to close.
		 * Done so that 1.0 clients (Lynx) can detect closure.
		 */
		if (request_get_version(request) != VERSION_11
		&& !connection_is_persistent(conn)
		&& strlen(response_get_connection(response)) == 0)
			response_set_connection(response, "close");

		cbSent = response_send(response, conn, e);
		http_server_add_logentry(srv, conn, request, response_get_status(response), cbSent);
		if (cbSent == 0)
			return 0;

		/* Did the user set the Connection header field to "close" */
		if (strcmp(response_get_connection(response), "close") == 0)
			return 1;

		if (!connection_is_persistent(conn))
			return 1;

		/*
		 * NOTE: Her må/bør vi legge inn ny funksjonalitet:
		 * Disconnect connections som
		 * a) Har kjørt lengst i tid
		 * b) Har overført mest bytes (opp eller ned)
		 * eller c) har dårligst transfer rate
		 * Grunnen er at dersom vi har n worker threads og n
		 * persistent connections, havner alle nye connections i kø.
		 * De får aldri kjøretid. Så disconnect-regelen over gjelder derfor
		 * kun om køen har > 0 entries.
		 */

		connection_flush(conn);
		request_recycle(request);
		response_recycle(response);
	}

	/* Shutdown detected */
	return 1;
}

int http_status_code(int error)
{
	return error >= HTTP_STATUS_MIN && error <= HTTP_STATUS_MAX;
}
