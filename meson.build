project('pmcp', 'cpp',
        license: 'MIT',
        default_options: ['buildtype=debug',
                          'cpp_std=c++20',
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
  'src/Server.cc',
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

if not cc.has_header('format')
  error('compiler with c++20 format support is required. gcc 13+ for ex.')
endif

if not cc.has_header('cpptoml.h')
  error('cpptoml not installed')
endif

deps = [
  cc.find_library('unistring'),
  cc.find_library('stdc++fs'),
  dependency('sndfile'),
  dependency('libpipewire-0.3'),
  dependency('taglib'),
]

glyr = dependency('libglyr', required: get_option('glyr'))
if glyr.found()
  add_project_arguments('-DENABLE_GLYR', language: 'cpp')
  deps += glyr
endif

executable('pmcp', files, dependencies: deps)

if get_option('ctl').enabled()
  executable('pmcpctl', ['src/ctl.cc'])
endif
