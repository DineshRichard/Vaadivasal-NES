cmake_minimum_required(VERSION 3.22.1)
project(vaadivasal C)

set(CORE fceumm/src)
set(LRC ${CORE}/drivers/libretro/libretro-common)

file(GLOB BOARD_SRC ${CORE}/boards/*.c)
file(GLOB INPUT_SRC ${CORE}/input/*.c)

set(CORE_SRC
    ${CORE}/drivers/libretro/libretro.c
    ${CORE}/drivers/libretro/libretro_dipswitch.c
    ${CORE}/cart.c ${CORE}/cheat.c ${CORE}/crc32.c ${CORE}/fceu-endian.c
    ${CORE}/fceu-memory.c ${CORE}/fceu.c ${CORE}/fds.c ${CORE}/fds_apu.c
    ${CORE}/file.c ${CORE}/filter.c ${CORE}/general.c ${CORE}/input.c
    ${CORE}/md5.c ${CORE}/nsf.c ${CORE}/palette.c ${CORE}/ppu.c ${CORE}/sound.c
    ${CORE}/state.c ${CORE}/video.c ${CORE}/vsuni.c ${CORE}/ines.c
    ${CORE}/unif.c ${CORE}/x6502.c ${CORE}/ntsc/nes_ntsc.c
    ${LRC}/streams/memory_stream.c
    ${LRC}/compat/compat_posix_string.c ${LRC}/compat/compat_snprintf.c
    ${LRC}/compat/compat_strcasestr.c ${LRC}/compat/compat_strl.c
    ${LRC}/compat/fopen_utf8.c ${LRC}/encodings/encoding_utf.c
    ${LRC}/file/file_path.c ${LRC}/file/file_path_io.c
    ${LRC}/streams/file_stream.c ${LRC}/streams/file_stream_transforms.c
    ${LRC}/string/stdstring.c ${LRC}/time/rtime.c ${LRC}/vfs/vfs_implementation.c
)

add_library(vaadivasal SHARED native-lib.c ${CORE_SRC} ${BOARD_SRC} ${INPUT_SRC})

target_compile_definitions(vaadivasal PRIVATE
    __LIBRETRO__ PATH_MAX=1024 FCEU_VERSION_NUMERIC=9900
    FRONTEND_SUPPORTS_RGB565 HAVE_NTSC_FILTER PSS_STYLE=1)
target_include_directories(vaadivasal PRIVATE
    ${CORE}/drivers/libretro ${LRC}/include ${CORE} ${CORE}/input
    ${CORE}/boards ${CORE}/ntsc)
target_compile_options(vaadivasal PRIVATE -O2 -w)
target_link_libraries(vaadivasal android jnigraphics log m)
target_link_options(vaadivasal PRIVATE -Wl,-z,max-page-size=16384)
