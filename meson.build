project('pmcp', 'cpp',
        license: 'MIT',
        default_options: ['buildtype=debug',
                          'cpp_std=c++23',
                          'warning_level=3'])

cc = meson.get_compiler('cpp')
files = [
  'src/main.cc',
  'src/utf8.cc',
  'src/ui.cc',
  'src/input.cc',
  'src/Terminal.cc',
  'src/Toml.cc',
  'src/Config.cc',
  'src/Theme.cc',
  'src/EventLoop.cc',
  'src/Keymap.cc',
  'src/Player.cc',
  'src/Source.cc',
  'src/Sink.cc',
  'src/Playqueue.cc',
  'src/Playlist.cc',
  'src/Scrollable.cc',
  'src/PlayerView.cc',
  'src/Help.cc',
  'src/Lyrics.cc'
]

if not cc.has_header('print')
  error('compiler with c++23 print support is required. gcc 14+ for ex.')
endif

deps = [
  cc.find_library('stdc++fs'),
  dependency('sndfile'),
  dependency('libpipewire-0.3'),
  dependency('taglib'),
  dependency('tomlplusplus')
]

if get_option('mpris').enabled()
  libsystemd = dependency('libsystemd', required: true)
  if libsystemd.found()
    add_project_arguments('-DENABLE_MPRIS', language: 'cpp')
    deps += libsystemd
    files += 'src/Mpris.cc'
  endif
endif

glyr = dependency('libglyr', required: get_option('glyr'))
if glyr.found()
  add_project_arguments('-DENABLE_GLYR', language: 'cpp')
  deps += glyr
endif

spectralizer = get_option('spectralizer')
if spectralizer == 'mkl'
  add_project_arguments('-DENABLE_SPECTRALIZER=SPECTRALIZER_BACKEND_MKL', language: 'cpp')
  deps += dependency('mkl-static-lp64-seq')
elif spectralizer == 'fftw'
  add_project_arguments('-DENABLE_SPECTRALIZER=SPECTRALIZER_BACKEND_FFTW', language: 'cpp')
  deps += dependency('fftw3')
endif

executable('pmcp', files, dependencies: deps, install: true)

if get_option('ctl').enabled()
  if get_option('mpris').enabled()
    executable('pmcpctl', ['src/ctl.cc'], dependencies: libsystemd, install: true)
  else
    message('pmcpctl needs mpris support, skipping')
  endif
endif

install_data(['share/config.toml','share/default_theme.toml','share/keymap.toml'])
