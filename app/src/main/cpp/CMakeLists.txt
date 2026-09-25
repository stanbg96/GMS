cmake_minimum_required(VERSION 3.22.1)
project("engine")

add_library(${CMAKE_PROJECT_NAME} SHARED engine.cpp)

# Добавяме GLESv3 за 3D графика и log за дебъгване
target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        log
        GLESv3)
