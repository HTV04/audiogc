# AudiOGC
A versatile audio engine written in C++ for GameCube and Wii homebrew.

[![GitHub release (latest SemVer including pre-releases)](https://img.shields.io/github/v/release/HTV04/audiogc?include_prereleases&style=flat-square)](https://github.com/HTV04/audiogc/releases) [![GitHub issues](https://img.shields.io/github/issues/HTV04/audiogc?style=flat-square)](https://github.com/HTV04/audiogc/issues) [![GitHub pull requests](https://img.shields.io/github/issues-pr/HTV04/audiogc?style=flat-square)](https://github.com/HTV04/audiogc/pulls) [![GitHub](https://img.shields.io/github/license/HTV04/audiogc?style=flat-square)](https://github.com/HTV04/audiogc/blob/main/LICENSE) [![Discord](https://img.shields.io/discord/852658576577003550?style=flat-square)](https://discord.gg/tQGzN2Wu48)

AudiOGC supports common audio formats, simultaneous playback, and other useful features, all without the headaches of working with audio streams and threads directly. It is designed to be easy to use and integrate into homebrew projects of any kind. AudiOGC also utilizes efficient [libraries](#libraries) for its operations, making it faster and more lightweight.

Supported formats:
* FLAC
* MP3
* (Ogg) Vorbis
* WAV

More formats may be added in the future.

Check out the [audio demo](https://github.com/HTV04/audiogc/releases/latest)!

## Installation
Before following these instructions, you will need to [install devkitPro and devkitPPC](https://devkitpro.org/wiki/Getting_Started).

### Pacman
The quickest way to install AudiOGC is by adding the `htv-dkp-libs` repository, allowing you to install AudiOGC and its dependencies from pacman. Follow the instructions [here](https://github.com/HTV04/htv-dkp-libs).

After setting up the repository, install the `wii-audiogc` and/or `gamecube-audiogc` package(s) via pacman.

### Manual installation
Alternatively, you can install AudiOGC and its dependencies manually.

* Download the [latest libogc-mod commit](https://github.com/HTV04/libogc-mod/actions) and copy its contents to `$DEVKITPRO`.
* Download the [latest version of AudiOGC](https://github.com/HTV04/audiogc/releases/latest) and copy its contents to `$DEVKITPRO/libogc-mod/portlibs`. Create the `portlibs` directory if it doesn't exist yet.

## Usage
Here's a quick example of how to use AudiOGC.

* Make sure the following libraries are linked in your Makefile:
```make
LIBS  +=  -laudiogc -laesnd -lfat
```

* Include the following header files in your project:
```c++
#include <aesndlib.h> // AESND
#include <fat.h> // libfat

#include <audiogc.hpp> // AudiOGC
```

* Next, initialize AESND (AudiOGC's backend), and libfat (for reading audio files from an SD card):
```c++
AESND_Init();
fatInitDefault();
```

* Finally, create a `player` object and play it:
```c++
audiogc::player my_player = audiogc::player(audiogc::types::detect, "sd:/path/to/audio/file");

my_player.play();
```

Refer to the [wiki](https://github.com/HTV04/audiogc/wiki) for more documentation.

## Building
Ensure [devkitPro and devkitPPC](https://devkitpro.org/wiki/Getting_Started) are installed before proceeding.

* Download the [latest libogc-mod commit](https://github.com/HTV04/libogc-mod/actions) and copy its contents to `$DEVKITPRO`.
* Clone this repo and run `make install` to build for the Wii, or `make PLATFORM=gamecube install` to build for the GameCube.
  * Omit `install` if you would just like to build AudiOGC without installing.
  * You will need to run `make clean` before switching platforms when compiling.

AudiOGC will be built and installed to `$DEVKITPRO/libogc-mod/portlibs/[platform]`.

### Using libogc or libogc2
It is recommended to use [libogc-mod](https://github.com/HTV04/libogc-mod) with AudiOGC because it has more fixes and features than the original libogc. However, if desired or necessary, you can build AudiOGC using libogc or libogc2.

Append `LIBOGC_VER=libogc` or `LIBOGC_VER=libogc2` to the `make` command to use libogc or libogc2 when building.

Install paths:
* libogc: `$DEVKITPRO/portlibs/[platform]`
* libogc2: `$DEVKITPRO/libogc2/portlibs/[platform]`

NOTE: The libogc2 path is technically unofficial, because it's not included in the PORTLIBS variable. However, it helps separate it from other versions that may be present. Be sure to add it to your Makefile:
```make
LIBDIRS :=  $(PORTLIBS) $(DEVKITPRO)/libogc2/portlibs/[platform]
```

## Libraries
AudiOGC uses [AESND](https://github.com/HTV04/libogc-mod/tree/main/libaesnd) as its audio backend. AESND's functions can be used in conjunction with AudiOGC as long as they are not modifying anything from AudiOGC's players.

The FLAC, MP3, and WAV formats are supported by their respective [dr_libs](https://github.com/mackron/dr_libs). The Vorbis format is supported by [stb_vorbis](https://github.com/nothings/stb/blob/master/stb_vorbis.c).

## Special thanks
* [Extrems](https://github.com/Extrems), for helping me out tons with issues I had during AudiOGC's development.
* [mackron](https://github.com/mackron), for developing [dr_libs](https://github.com/mackron/dr_libs) and helping me figure out issues I was having with the libraries.
* [nothings](https://github.com/nothings), for developing [stb_vorbis](https://github.com/nothings/stb/blob/master/stb_vorbis.c).
* [devkitPro](https://devkitpro.org/), for making GameCube and Wii homebrew possible. ❤
