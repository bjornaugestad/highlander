project('highlander', ['c'], version: '0.9.99', default_options: ['warning_level=2'])

# Basic compiler
cc = meson.get_compiler('c')
global_includes = []
global_c_args = []
global_link_args = []
openssl_dep = dependency('openssl', required: true)
threads     = dependency('threads', required: true)

subdir('tools')
subdir('meta')
subdir('http')
subdir('apps')
