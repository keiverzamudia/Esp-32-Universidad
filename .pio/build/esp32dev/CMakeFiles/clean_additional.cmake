# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "app.css.S"
  "app.js.S"
  "bootloader\\bootloader.bin"
  "bootloader\\bootloader.elf"
  "bootloader\\bootloader.map"
  "config\\sdkconfig.cmake"
  "config\\sdkconfig.h"
  "esp-idf\\esptool_py\\flasher_args.json.in"
  "esp32.bin"
  "esp32.map"
  "ev01_full.jpg.S"
  "ev01_thumb.jpg.S"
  "ev02_full.jpg.S"
  "ev02_thumb.jpg.S"
  "ev03_full.jpg.S"
  "ev03_thumb.jpg.S"
  "ev04_full.jpg.S"
  "ev04_thumb.jpg.S"
  "ev05_full.jpg.S"
  "ev05_thumb.jpg.S"
  "ev06_full.jpg.S"
  "ev06_thumb.jpg.S"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "index.html.S"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "manifest.json.S"
  "project_elf_src_esp32.c"
  )
endif()
