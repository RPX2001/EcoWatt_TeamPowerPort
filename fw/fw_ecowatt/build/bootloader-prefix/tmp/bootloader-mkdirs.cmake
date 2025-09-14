# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.3.1/components/bootloader/subproject"
  "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader"
  "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix"
  "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix/tmp"
  "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix/src"
  "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Documents/UoM/Semester 7/Embedded/Firmware/fw/fw_ecowatt/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
