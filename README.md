# TTHUMP - A Thumbnailer Library

**NOT YET FUNCTIONAL**

An easy-to-use thumbnailing library conforming to the freedesktop thumbnailing
specification (no warranty.)

## Requirements

- meson 0.56.0+
- c11 support
- libc support c11 threads or pthreads
- libc support for X/Open 6 (superset of POSIX 2001+)
- FFmpeg 5.0+ libs (libavutil, libavformat, libswscale, libavcodec)

If you are on any kind of non-embedded Linux or BSD based system you
will likely fulfil the requirements besides ffmpeg out of the box.

If the FFmpeg dependencies are not satisfied by your system they will be built
as a submodule.

## Build

    meson setup build
    ninja -Cbuild

TThump can be installed via `meson install`. For example locally

    meson configure build --prefix ~/.local/
    meson install -Cbuild

The library will be installed below `~/.local` same as it would be under `/`.
If installed prefixed there, you will likely at the very least need to
configure PKG\_CONFIG\_PATH for it to be found by other projects.

**Note:** The ffmpeg wrap fallback is not a real meson port but rather builds
the binaries with the original ffmpeg configure script before statically
linking them to tthump

## Unit tests

    meson setup build
    meson test -Cbuild

## Usage

See tthump.h

## TODO

- Release 1.0.0
- Implement extended API
- Remove X/Open System Interface requirement and even make POSIX optional

Tests:

- Missing Thumbnail
- Missing uri/mtime in thumbnail -> Recreate
- Invalid MTime field -> Recreate
- Hash collission -> Recreate
- Older mtime in thumbnail -> Recreation
- No recreation on valid

## Contributing

Feel free to post an issue, MR or to send a private message/mail.

## License

LGPLv3
