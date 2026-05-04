# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/nexland/esp/esp-idf/components/bootloader/subproject"
  "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader"
  "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix"
  "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix/tmp"
  "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix/src/bootloader-stamp"
  "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix/src"
  "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf/build-internal/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
