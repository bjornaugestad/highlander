/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */


#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <assert.h>

#include <meta_misc.h>

#include "internals.h"

/* Local helper functions */
static int check_attributes(http_request request, page_attribute a)
{
    const char *page_val;

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
static status_t send_disk_file(
    http_server srv,
    connection conn,
    http_request req,
    http_response response,
    error e)
{
    size_t i;
    const char *uri, *docroot;
    char filename[CCH_URI_MAX + DOCUMENTROOT_MAX + 2];
    struct stat st;
    page_attribute a;
    const char *content_type;

    assert(srv != NULL);
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
    if ((docroot = http_server_get_documentroot(srv)) == NULL)
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
    if (i >= sizeof filename)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    strcpy(filename, docroot);
    strcat(filename, "/");
    strcat(filename, uri);

    /* Does the file exist? */
    if (stat(filename, &st))
        return set_http_error(e, HTTP_404_NOT_FOUND);

    if (S_ISREG(st.st_mode))
        ;
    else if (S_ISDIR(st.st_mode)) {
        /* BUG? If docroot+uri+index.html > sizeof filename we have issues */
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

    if ((a = http_server_get_default_attributes(srv)) != NULL
    && !check_attributes(req, a)) {
        response_set_status(response, HTTP_406_NOT_ACCEPTABLE);
        return set_http_error(e, HTTP_406_NOT_ACCEPTABLE);
    }

    content_type = get_mime_type(filename);

    /* This function does not actually send the file, just stats it
     * and stores the path. The contents will be sent later when
     * response_send_entity is called. */
    // TODO? set entitylen too? We need it when logging. boa20141212
    if (!response_send_file(response, filename, content_type, e))
        return 0;

    response_set_status(response, HTTP_200_OK);
    return success;
}

/* Call the callback function for the page */
status_t handle_dynamic(
    connection conn,
    http_server srv,
    dynamic_page p,
    http_request req,
    http_response response,
    error e)
{
    int status = HTTP_200_OK;
    http_version version;

    assert(p != NULL);

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
     * for RFC2617 HTTP Authentication */
    request_set_connection(req, conn);
    response_set_version(response, version);
    response_set_last_modified(response, time(NULL));

    /* Run the dynamic function. It is supposed to return 0 for OK,
     * but we accept any legal HTTP status code. Illegal status codes
     * are mapped to 500. */
    if ((status = dynamic_run(p, req, response)))
        status = http_status_code(status) ? status : HTTP_500_INTERNAL_SERVER_ERROR;
    else
        status = HTTP_200_OK;

    response_set_status(response, status);
    return success;
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

static status_t serviceConnection2(
    http_server srv,
    connection conn,
    http_request request,
    http_response response,
    error e)
{
    dynamic_page dp;
    size_t cbSent;
    int status, iserror = 0;
    size_t max_posted_content = http_server_get_post_limit(srv);

    while (!http_server_shutting_down(srv)) {
        if (!data_on_socket(conn))
            return set_tcpip_error(e, EAGAIN);

        /* Were we able to read a valid http request?
         * If not, what is the cause of the error? If it is a http
         * protocol error, we try to send a response back to the client
         * and close the connection. If it is anything else(tcp/ip, os)
         * we stop processing.  */
        iserror = !request_receive(request, conn, max_posted_content, e);

        /* So far, so good. We have a valid HTTP request.
         * Now see if we can locate a page handler function for it.
         * If we do, call it. If not, see if it on disk or if the
         * http_server has a default page handler. If neither is true,
         * then the page was not found(404).
         */
        if (iserror)
            ;
        else if ((dp = http_server_lookup(srv, request)) != NULL) {
            if (!handle_dynamic(conn, srv, dp, request, response, e))
                iserror = 1;
        }
        else if (http_server_can_read_files(srv)) {
            if (!send_disk_file(srv, conn, request, response, e))
                iserror = 1;
        }
        else if (http_server_has_default_page_handler(srv)) {
            if (!http_server_run_default_page_handler(conn, srv, request, response, e))
                iserror = 1;
        }
        else {
            /* We didn't find the page */
            response_set_status(response, HTTP_404_NOT_FOUND);
            if (!response_set_connection(response, "close"))
                return failure;
        }

        if (iserror) {
            if (is_protocol_error(e)) {
                status = get_error_code(e);
                response_set_status(response, status);

                if (!response_set_connection(response, "close"))
                    return failure;

                if (!response_send(response, conn, e, &cbSent))
                    return failure;

                http_server_add_logentry(srv, conn, request, status, cbSent);
            }

            return failure;
        }

        /* Some extra stuff for HTTP 1.0 clients. If client is 1.0
         * and connection_close() == 1 and connection header field
         * isn't set, then we set the connection flag to close.
         * Done so that 1.0 clients (Lynx) can detect closure.  */
        if (request_get_version(request) != VERSION_11
        && !connection_is_persistent(conn)
        && strlen(response_get_connection(response)) == 0) {
            if (!response_set_connection(response, "close"))
                return failure;
        }

        if (!response_send(response, conn, e, &cbSent))
            return failure;

        http_server_add_logentry(srv, conn, request, response_get_status(response), cbSent);
        if (cbSent == 0)
            return failure;

        /* Did the user set the Connection header field to "close" */
        if (strcmp(response_get_connection(response), "close") == 0)
            return success;

        if (!connection_is_persistent(conn))
            return success;

        /*
         * NOTE: Her m�/b�r vi legge inn ny funksjonalitet:
         * Disconnect connections som
         * a) Har kj�rt lengst i tid
         * b) Har overf�rt mest bytes (opp eller ned)
         * eller c) har d�rligst transfer rate
         * Grunnen er at dersom vi har n worker threads og n
         * persistent connections, havner alle nye connections i k�.
         * De f�r aldri kj�retid. S� disconnect-regelen over gjelder derfor
         * kun om k�en har > 0 entries.
         */

        if (!connection_flush(conn))
            warning("Could not flush connection\n");

        request_recycle(request);
        response_recycle(response);
    }

    /* Shutdown detected */
    return success;
}

void* serviceConnection(void* psa)
{
    connection conn = psa;
    status_t ok;
    http_server srv;
    http_request request;
    http_response response;

    error e = error_new();
    if (e == NULL)
        return NULL;

    srv =  connection_arg2(conn);
    request = http_server_get_request(srv);
    request_set_defered_read(request, http_server_get_defered_read(srv));
    response = http_server_get_response(srv);

    ok = serviceConnection2(srv, conn, request, response, e);
    if (!ok && is_tcpip_error(e))
        connection_discard(conn);
    else if (!connection_close(conn))
        warning("Could not close connection\n");

    http_server_recycle_request(srv, request);
    http_server_recycle_response(srv, response);

    error_free(e);
    return (void*)(intptr_t)ok;
}

int http_status_code(int iserror)
{
    return iserror >= HTTP_STATUS_MIN && iserror <= HTTP_STATUS_MAX;
}
