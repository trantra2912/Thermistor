# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/User/ESP/Espressif/frameworks/esp-idf-v4.4.3/components/bootloader/subproject"
  "D:/User/ESP/dientronhiet/build/bootloader"
  "D:/User/ESP/dientronhiet/build/bootloader-prefix"
  "D:/User/ESP/dientronhiet/build/bootloader-prefix/tmp"
  "D:/User/ESP/dientronhiet/build/bootloader-prefix/src/bootloader-stamp"
  "D:/User/ESP/dientronhiet/build/bootloader-prefix/src"
  "D:/User/ESP/dientronhiet/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/User/ESP/dientronhiet/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
