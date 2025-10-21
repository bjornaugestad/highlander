project('highlander', ['c'], version: '0.9.99', default_options: ['warning_level=2'])

# Basic compiler
cc = meson.get_compiler('c')
global_includes = []
global_c_args = [
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Werror',
    '-std=gnu2x',
    '-Wstrict-prototypes', 
    '-Wmissing-prototypes',
    '-Wwrite-strings',
    '-Wshadow',
    '-Wcast-align',
    '-Wpointer-arith',
    '-Wformat-security',
    '-Wdouble-promotion',
    '-Wuninitialized',
]

# gcc/clang issues
if cc.get_id() == 'gcc'
  global_c_args += [
    '-Warith-conversion',
    '-Wlogical-op',
    '-Wduplicated-cond',
    '-Wduplicated-branches',
    '-Wstringop-overflow',
    '-Wrestrict',
  ]
elif cc.get_id() == 'clang'
  global_c_args += [
    '-Wimplicit-int-conversion',  
    '-Wshorten-64-to-32',
    '-Wconversion',
    '-Wsign-conversion'
  ]
endif



global_link_args = []
openssl_dep = dependency('openssl', required: true)
threads     = dependency('threads', required: true)
seccomp_dep = dependency('libseccomp', required : true)

subdir('tools')
subdir('meta')
subdir('http')
subdir('apps')
