# pmcp

[![Build Status](https://img.shields.io/github/actions/workflow/status/okeri/pmcp/ci.yml?branch=master)](https://github.com/okeri/pmcp/actions) [![MIT](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)

PW music console player.
Heavily inspired by [MOC](http://moc.daper.net), but much less feature-rich :)

## dependencies
[pipewire](https://pipewire.org)(NOTE: version 1.6 minimum)  
[taglib](https://taglib.org)  
[libsndfile](https://libsndfile.github.io/libsndfile)  
[libglyr](https://github.com/sahib/glyr)(optional for lyrics fetching)  
[libsystemd](https://systemd.io)(optional for mpris remote control)  
[intel-oneapi-mkl](https://software.intel.com/content/www/us/en/develop/tools/oneapi.html)(optional for visualization)  
[fftw](http://www.fftw.org)(optional for visualization)  


## build dependencies
C++23 compiler  
[meson](https://mesonbuild.com)  
[ninja](https://ninja-build.org)  
[toml++](https://marzer.github.io/tomlplusplus)  

## building
```console
meson setup -Dspectralizer=mkl --prefix=/usr build
meson install -C build
```

## configuring
pmcp looks up for config files in $XDG_CONFIG_HOME/pmcp (typically in ~/.config/pmcp).  
Config file examples you may find [here](https://github.com/okeri/pmcp/tree/master/share)

## remote control
pmcp appears on the dbus session bus as an
[MPRIS2](https://specifications.freedesktop.org/mpris-spec/latest) player
(`org.mpris.MediaPlayer2.pmcp`), so media keys, `playerctl` and status bars work
out of the box. `pmcpctl` ships as a small client for the same interface:

```console
pmcpctl <status|monitor|play|pause|next|prev|stop|quit>
```

`pmcpctl status` prints the current song and position, `pmcpctl monitor` prints
it again on every change and sleeps in between, which makes it a cheap feed for
a status bar.

