# Auto-converted from Makefile.am (best-effort)

# Installed headers
install_headers('rfc1738.h', 'highlander.h', 'meta_html.h')


highlander_inc = include_directories('.')
highlander_lib = static_library('highlander', [
    'http.c', 
    'rfc1738.c', 
    'attribute.c', 
    'cookies.c', 
    'dynamic_page.c', 
    'highlander.c', 
    'html_buffer.c', 
    'html_menu.c', 
    'html_section.c', 
    'html_template.c', 
    'html_template_repository.c', 
    'http_server.c', 
    'http_client.c', 
    'parse_http.c', 
    'parse_time.c', 
    'request.c', 
    'response.c', 
    'general_header.c', 
    'entity_header.c', 
    'send_status_code.c'
],
  install: true,
  include_directories: [meta_inc],
  c_args: global_c_args
)

highlander_dep = declare_dependency(
  link_with: highlander_lib,
  include_directories: highlander_inc,
)

# felles http-kilder
http_common = [
  'http.c','request.c','response.c','general_header.c','entity_header.c',
  'cookies.c','parse_http.c','parse_time.c','dynamic_page.c','rfc1738.c',
  'send_status_code.c','attribute.c'
]

# server-test
http_server_check = executable(
  'http_server_check',
  ['http_server.c','http_client.c','highlander.c'] + http_common,
  c_args: ['-DCHECK_HTTP_SERVER'],
  dependencies: [meta_dep, openssl_dep, threads],
)
test('http_server_check', http_server_check)

# client-test
http_client_check = executable(
  'http_client_check',
  ['http_client.c'] + http_common,
  c_args: ['-DCHECK_HTTP_CLIENT'],
  dependencies: [meta_dep, openssl_dep, threads],
)
test('http_client_check', http_client_check)



