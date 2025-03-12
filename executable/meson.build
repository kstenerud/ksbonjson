project(
  'bonjson_exe',
  'c',
  version : '1.0.0',
  license : 'MIT',
  default_options : [
    'warning_level=2',
    'optimization=3',
  ]
)

cmake = import('cmake')
jsonc_proj = cmake.subproject('json-c')
jsonc_lib = jsonc_proj.dependency('json-c')


project_source_files = [
  'src/main.c'
]

project_dependencies = [
  dependency('bonjson', fallback : ['bonjson', 'bonjson_dep']),
  jsonc_lib
]

build_args = [
]


# ===================================================================

# ======
# Target
# ======

build_args += [
  '-DPROJECT_NAME=' + meson.project_name(),
  '-DPROJECT_VERSION=' + meson.project_version(),
  '-Wno-error=suggest-attribute=pure',
  '-Wno-error=suggest-attribute=const',
  '-Wno-error=suggest-attribute=noreturn',
  '-Wno-error=suggest-attribute=format',
  '-Wno-error=suggest-attribute=cold',
  '-Wno-error=suggest-attribute=malloc',
]

project_target = executable(
  'bonjson',
  project_source_files,
  dependencies: project_dependencies,
  install : true,
  c_args : build_args,
)

test('basic', project_target)
