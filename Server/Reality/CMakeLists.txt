cmake_minimum_required(VERSION 3.20)
project(Reality LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Defines
add_compile_definitions(NDEBUG _CONSOLE _CRT_SECURE_NO_DEPRECATE BOOST_TIMER_ENABLE_DEPRECATED NOMINMAX)
if(WIN32)
    add_compile_definitions(WIN32)
    if(MSVC)
        add_compile_options(/Zc:twoPhase-)
    endif()
endif()

# Sockets Library
file(GLOB SOCKETS_SRC 
    "../Dependencies10/Sockets/*.cpp"
)
list(REMOVE_ITEM SOCKETS_SRC 
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies10/Sockets/StdoutLog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies10/Sockets/Sockets-config.cpp"
)

add_library(Sockets STATIC ${SOCKETS_SRC})
target_include_directories(Sockets PUBLIC "../Dependencies10/Sockets")
if(MSVC)
    target_compile_options(Sockets PRIVATE /W3 /wd4244 /wd4267)
    target_compile_definitions(Sockets PRIVATE _HAS_STD_BYTE=0)
else()
    target_compile_options(Sockets PRIVATE -Wno-deprecated -Wno-narrowing -fpermissive)
endif()

# Lua 5.4 Library
file(GLOB LUA_SRC 
    "../Dependencies10/Lua/*.c"
)
list(REMOVE_ITEM LUA_SRC 
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies10/Lua/lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies10/Lua/luac.c"
)
add_library(Lua STATIC ${LUA_SRC})
target_include_directories(Lua PUBLIC "../Dependencies10/Lua")
if(MSVC)
    target_compile_options(Lua PRIVATE /W3 /wd4244 /wd4334)
endif()

# CryptoPP Library (On Linux/UNIX, use system libcrypto++-dev)
if(WIN32)
    file(GLOB CRYPTOPP_SRC 
        "../Dependencies10/CryptoPP/*.cpp"
    )
    list(FILTER CRYPTOPP_SRC EXCLUDE REGEX "test|bench|validat")

    add_library(CryptoPP STATIC ${CRYPTOPP_SRC})
    target_include_directories(CryptoPP PUBLIC "../Dependencies10/CryptoPP")
    if(MSVC)
        target_compile_options(CryptoPP PRIVATE /wd4244 /wd4267 /wd4100 /wd4127 /wd4505 /wd4530)
        target_compile_definitions(CryptoPP PRIVATE CRYPTOPP_DISABLE_ASM=1 _HAS_STD_BYTE=0)
    endif()
endif()

# Reality Engine
file(GLOB_RECURSE REALITY_SRC "Source/*.cpp")
file(GLOB_RECURSE REALITY_INC "Source/*.h")

set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "Build demo" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
add_subdirectory("../Dependencies10/recastnavigation" "recastnavigation_build")

add_executable(Reality ${REALITY_SRC} ${REALITY_INC})

target_include_directories(Reality PRIVATE
    "$ENV{BOOST_ROOT}"
    "E:/Games/The Matrix Online/boost/boost_1_84_0"
    "../Dependencies10/CryptoPP"
    "../Dependencies10/Sockets/Project.net/Include"
    "../Dependencies10/Sockets"
    "../Dependencies10/MySQL/include"
    "../Dependencies10/recastnavigation/Recast/Include"
    "../Dependencies10/recastnavigation/Detour/Include"
    "../Dependencies10/recastnavigation/DetourCrowd/Include"
    "../Dependencies10/Lua"
    "Source"
)

if(NOT WIN32)
    target_include_directories(Reality PRIVATE "/usr/include/crypto++")
endif()

if(WIN32)
    target_link_directories(Reality PRIVATE
        "../Dependencies10/MySQL/lib/64"
        "$ENV{BOOST_ROOT}/stage/lib"
        "E:/Games/The Matrix Online/boost/boost_1_84_0/stage/lib"
    )
    target_link_libraries(Reality PRIVATE
        Sockets
        CryptoPP
        Lua
        libmysql
        Recast
        Detour
        DetourCrowd
        Dbghelp
        Winmm
    )
else()
    target_link_libraries(Reality PRIVATE
        Sockets
        cryptopp
        Lua
        mysqlclient
        Recast
        Detour
        DetourCrowd
        pthread
        dl
    )
endif()

if(MSVC)
    target_compile_options(Reality PRIVATE /W3 /MP /bigobj)
    set_target_properties(Reality PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_target_properties(Reality PROPERTIES LINK_FLAGS /OPT:NOREF)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MAP")
else()
    target_compile_options(Reality PRIVATE -O3 -fpermissive -Wno-deprecated -Wno-narrowing)
endif()

set_target_properties(Reality PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries")

add_custom_command(TARGET Reality POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Data/abilities.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Data/abilities.json"
)
