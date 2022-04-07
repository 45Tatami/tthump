#!/usr/bin/env sh

set -e
set -x

[ -z "${MESON_BUILD_ROOT}" ]  && (echo "Needs to be run by meson"; exit 1)
[ -z "${MESON_SOURCE_ROOT}" ] && (echo "Needs to be run by meson"; exit 1)
[ -z "${MESON_SUBDIR}" ]      && (echo "Needs to be run by meson"; exit 1)

INSTALL_PATH="${MESON_SOURCE_ROOT}/${MESON_SUBDIR}/ffmpeg_build"

rm -rf "${INSTALL_PATH}" || true
mkdir "${INSTALL_PATH}"

cd "${MESON_SOURCE_ROOT}/${MESON_SUBDIR}"
./configure --prefix="${INSTALL_PATH}" \
        --disable-doc \
        --disable-programs \
        --disable-autodetect \
        --enable-avcodec \
        --enable-avformat \
        --enable-swscale \
        --enable-swresample \
        --enable-libvpx \
        --enable-zlib \
        --pkg-config-flags="--static" \
        --enable-pic \
        --disable-shared \
        --enable-static
make -j
make install
