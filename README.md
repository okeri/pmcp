# pmcp

[![Build Status](https://img.shields.io/github/actions/workflow/status/okeri/pmcp/ci.yml?branch=master)](https://github.com/okeri/pmcp/actions) [![MIT](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)

PW music console player.
Heavily inspired by [MOC](http://moc.daper.net), but much less feature-rich :)

## dependencies
[pipewire](https://pipewire.org)  
[taglib](https://taglib.org)  
[libsndfile](https://libsndfile.github.io/libsndfile)  
[libglyr](https://github.com/sahib/glyr)(optional for lyrics fetching)  


## build dependencies
C++20 compiler  
[meson](https://mesonbuild.com)  
[ninja](https://ninja-build.org)  
[cpptoml](https://github.com/skystrife/cpptoml)  

## building
```console
meson --prefix=/usr build
meson install -C build
```

## configuring
pmcp looks up for config files in $XDG_CONFIG_HOME/pmcp (typically in ~/.config/pmcp).
config file examples you may find [here](https://github.com/okeri/pmcp/tree/master/share)

