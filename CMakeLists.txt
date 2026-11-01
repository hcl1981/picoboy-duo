# PicoBoy Duo - Zweispieler-Sammlung fuer den (alten) PicoBoy ueber USB-C
#
#   mkdir build && cd build && cmake .. && make -j
#   -> build/pbduo.uf2
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)            # PicoBoy = RP2040 mit 2 MB Flash, Pico-kompatibel
include(pico_sdk_import.cmake)

project(pbduo C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(U8G2 third_party/u8g2)
add_executable(pbduo
    src/main.cpp
    src/multi.cpp
    src/settings.cpp
    src/starscreen.cpp
    src/picopac/picopac.cpp
    src/picopac/arduino.cpp
    src/hw.cpp
    src/gfx.cpp
    src/link.cpp
    src/play.cpp
    src/games/table.cpp
    src/games/tron.cpp
    src/games/pong.cpp
    src/games/duel.cpp
    src/games/arty.cpp
    src/games/bomber.cpp
    src/games/chess.cpp
    src/games/wnr.cpp
    src/games/c4.cpp
    src/games/dots.cpp
    jumpnbump/src/game.cpp
    jumpnbump/src/jnb.cpp
    jumpnbump/src/level.cpp
    ${U8G2}/u8g2_arc.c ${U8G2}/u8g2_bitmap.c ${U8G2}/u8g2_box.c ${U8G2}/u8g2_buffer.c
    ${U8G2}/u8g2_circle.c ${U8G2}/u8g2_cleardisplay.c ${U8G2}/u8g2_font.c ${U8G2}/u8g2_hvline.c
    ${U8G2}/u8g2_intersection.c ${U8G2}/u8g2_kerning.c ${U8G2}/u8g2_line.c ${U8G2}/u8g2_ll_hvline.c
    ${U8G2}/u8g2_polygon.c ${U8G2}/u8g2_setup.c ${U8G2}/u8x8_8x8.c ${U8G2}/u8x8_byte.c
    ${U8G2}/u8x8_cad.c ${U8G2}/u8x8_display.c ${U8G2}/u8x8_gpio.c ${U8G2}/u8x8_setup.c
    ${U8G2}/u8x8_string.c ${U8G2}/u8x8_u16toa.c ${U8G2}/u8x8_u8toa.c
    ${U8G2}/u8x8_d_ssd1306_128x64_noname.c ${U8G2}/u8g2_picoboy_setup.c ${U8G2}/picopac_fonts.c
)

# Stapel 4 KB (ganze SCRATCH_Y-Bank) statt 2 KB. Die grossen Felder der Demo
# und des Startmenues sind static gemacht; der Rest der PicoPac-Spiele braucht
# wenig. PICO_USE_STACK_GUARDS macht einen Ueberlauf zum sofortigen Fehler.
target_compile_definitions(pbduo PRIVATE PICO_STACK_SIZE=0x1000 PICO_USE_STACK_GUARDS=1)

target_include_directories(pbduo PRIVATE src src/picopac picopac jumpnbump/src ${U8G2})
target_compile_options(pbduo PRIVATE -O2 -Wall -Wno-unused-function
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)

# Die PicoPac-Spiele sind unveraendert uebernommener Arduino-Code; ihre
# Warnungen (Vergleiche mit Vorzeichen, ungenutzte Variablen) bleiben stehen.
set_source_files_properties(src/picopac/picopac.cpp PROPERTIES COMPILE_OPTIONS "-w")
set_source_files_properties(${U8G2}/picopac_fonts.c PROPERTIES COMPILE_OPTIONS "-w")

target_link_libraries(pbduo
    pico_stdlib
    hardware_flash
    hardware_spi
    hardware_pwm
    hardware_sync
    hardware_resets
    hardware_clocks
)

# Der USB-Port gehoert dem Spiele-Link: kein stdio ueber USB (und keins ueber UART).
pico_enable_stdio_usb(pbduo 0)
pico_enable_stdio_uart(pbduo 0)

pico_add_extra_outputs(pbduo)
