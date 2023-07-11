cmake_minimum_required(VERSION 3.15)
message("CMake version :" ${CMAKE_VERSION})
set(CMAKE_C_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64/cl.exe")

# set the project name
project(AIlarmaServer)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)


set(TARGET_COMPILE_DEFS)
set(TARGET_NAME ${PROJECT_NAME})

include(../../conan/server/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(${PROJECT_SOURCE_DIR})

set(HEADERS ${HEADERS} ${PROJECT_SOURCE_DIR}/media_video.h
    ${PROJECT_SOURCE_DIR}/BaseBuffer.h
    ${PROJECT_SOURCE_DIR}/basictypes.h)

set(SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/media_video.cpp
		       ${PROJECT_SOURCE_DIR}/main.cpp)



add_executable(${TARGET_NAME} ${HEADERS} ${SOURCES})
target_link_libraries(${TARGET_NAME} ${CONAN_LIBS})
