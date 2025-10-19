helloworld_exe = executable('helloworld', ['main.c'],
  install: true,
  include_directories: [ meta_inc, highlander_inc, global_includes],
  c_args: global_c_args,
  dependencies: [highlander_dep, meta_dep, openssl_dep],
  link_args: global_link_args
)

# Use a bash script to run the binary executable
helloworld_check = find_program('helloworld_check', dirs: meson.current_source_dir())
test(
  'helloworld_check',
  helloworld_check,
  args: [helloworld_exe.full_path()],   # sends full path to exe inn i scriptet
  workdir: meson.current_build_dir(),   #  eller en annen dir hvis scriptet forventer det
  is_parallel: false,                  
  timeout: 60,
)

