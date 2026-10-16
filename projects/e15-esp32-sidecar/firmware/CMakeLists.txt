# projects/e15-esp32-sidecar/firmware/CMakeLists.txt: an ESP-IDF project, built with
# idf.py in skyhorizon's WSL (never on aquamarine). The target is esp32 (the
# SBC-NodeMCU-ESP32 carries the original ESP32); idf.py set-target esp32 once.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(e15_sidecar)
