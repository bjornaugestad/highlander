helloworld_exe = executable('helloworld', ['main.c'],
  install: true,
  include_directories: [ meta_inc, highlander_inc, global_includes],
  c_args: global_c_args,
  dependencies: [highlander_dep, meta_dep, openssl_dep],
  link_args: global_link_args
)
