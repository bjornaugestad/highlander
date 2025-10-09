#include <stdio.h>
#include <unistd.h>

// Move to lib eventually
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <stdbool.h>

#include <tcp_server.h>
#include <connection.h>
#include <meta_process.h>
#include <miscssl.h>

// Default type is SSL. We may enable TCP instead.
static int m_servertype = SOCKTYPE_SSL;

static void* fn(void* arg)
{
    connection c = arg;
    char buf[1024];

    while (connection_gets(c, buf, sizeof buf)) {
        if (!connection_puts(c, buf) || !connection_flush(c))
            warning("Could not echo input.\n");
    }

    return NULL;
}

static void parse_command_line(int argc, char *argv[])
{
    const char *options = "t";

    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 't':
                m_servertype = SOCKTYPE_TCP;
                break;

            case '?':
            default:
                fprintf(stderr, "USAGE: %s [-t] where -t disables ssl(enables TCP)\n", argv[0]);
                exit(1);
        }
    }
}



// Move to tcp_server later
X509 *cert_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    X509 *crt = PEM_read_X509(fp, NULL, NULL, NULL);
    fclose(fp);
    return crt;
}

void cert_free(X509 *crt)
{
    if (crt)
        X509_free(crt);
}

bool cert_not_yet_valid(const X509 *crt)
{
    return X509_cmp_current_time(X509_get0_notBefore(crt)) > 0;
}

bool cert_expired(const X509 *crt)
{
    return X509_cmp_current_time(X509_get0_notAfter(crt)) <= 0;
}

bool cert_time_valid(const X509 *crt)
{
    return !cert_not_yet_valid(crt) && !cert_expired(crt);
}

/* --- usage --- */
bool cert_is_ca(const X509 *crt)
{
    int crit = -1;
    BASIC_CONSTRAINTS *bc = X509_get_ext_d2i(crt, NID_basic_constraints, &crit, NULL);
    bool ret = (bc && bc->ca);
    BASIC_CONSTRAINTS_free(bc);
    return ret;
}

bool cert_valid_for_server(const X509 *crt)
{
    return X509_check_purpose((X509 *)crt, X509_PURPOSE_SSL_SERVER, 0) == 1;
}

/* --- composite check --- */
bool cert_is_server_usable(const X509 *crt)
{
    return cert_time_valid(crt) && cert_valid_for_server(crt) && !cert_is_ca(crt);
}

EVP_PKEY *private_key_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return NULL;

    EVP_PKEY *res = PEM_read_PrivateKey(f, NULL, NULL, NULL);
    fclose(f);

    return res;
}

static void verify_key_and_cert(const char *server_key, const char *server_cert_chain)
{
    X509 *cert = cert_load(server_cert_chain);
    if (cert == NULL)
        die("%s : could not load\n", server_cert_chain);

    if (cert_not_yet_valid(cert))
        die("%s: Cert not yet valid\n", server_cert_chain);

    if (cert_expired(cert))
        die("%s: Cert has expired\n", server_cert_chain);

    if (cert_is_ca(cert))
        die("%s: Dude, don't use CA certs\n", server_cert_chain);

    // See if cert and private key belongs together
    EVP_PKEY *pkey = private_key_load(server_key);
    if (pkey == NULL)
        die("%s : could not load\n", server_key);

    if (X509_check_private_key(cert, pkey) != 1)
        die("%s and %s does not belong together\n", server_cert_chain, server_key);

    cert_free(cert);
    EVP_PKEY_free(pkey);
}

int main(int argc, char *argv[])
{
    process p;
    tcp_server srv;

    parse_command_line(argc, argv);

    if (m_servertype == SOCKTYPE_SSL && !openssl_init())
        exit(1);

    p = process_new("echoserver");
    srv = tcp_server_new(m_servertype);
    tcp_server_set_port(srv, 3000);

    if (!tcp_server_init(srv))
        exit(2);

    // New stuff. Verify that certs are OK.
    const char *server_cert_chain = "pki/server/server_chain.pem";
    const char *server_key = "pki/server/server.key";

    verify_key_and_cert(server_key, server_cert_chain);

    tcp_server_set_cert_chain_file(srv, server_cert_chain);
    tcp_server_set_private_key(srv, server_key);
    tcp_server_set_service_function(srv, fn, NULL);
    tcp_server_start_via_process(p, srv);

    if (!process_start(p, 0))
        exit(3);

    if (!process_wait_for_shutdown(p)) {
        perror("process_wait_for_shutdown");
        exit(4);
    }

    tcp_server_free(srv);
    process_free(p);
    return !!openssl_exit();
}
