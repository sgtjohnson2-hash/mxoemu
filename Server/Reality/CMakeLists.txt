cmake_minimum_required(VERSION 3.20)
project(Reality LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Defines
add_compile_definitions(WIN32 NDEBUG _CONSOLE _CRT_SECURE_NO_DEPRECATE BOOST_TIMER_ENABLE_DEPRECATED)

# Sockets Library
file(GLOB SOCKETS_SRC 
    "../Dependencies10/Sockets/*.cpp"
)
# Exclude tests and examples from Sockets
list(REMOVE_ITEM SOCKETS_SRC 
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies10/Sockets/StdoutLog.cpp"
)

add_library(Sockets STATIC ${SOCKETS_SRC})
target_include_directories(Sockets PUBLIC "../Dependencies10/Sockets")
if(MSVC)
    target_compile_options(Sockets PRIVATE /W3 /wd4244 /wd4267)
endif()

# CryptoPP Library
file(GLOB CRYPTOPP_SRC 
    "../Dependencies10/CryptoPP/*.cpp"
)
# Exclude tests
list(FILTER CRYPTOPP_SRC EXCLUDE REGEX "test|bench|validat")

add_library(CryptoPP STATIC ${CRYPTOPP_SRC})
target_include_directories(CryptoPP PUBLIC "../Dependencies10/CryptoPP")
if(MSVC)
    target_compile_options(CryptoPP PRIVATE /wd4244 /wd4267 /wd4100 /wd4127 /wd4505 /wd4530)
endif()

# Reality Engine
file(GLOB_RECURSE REALITY_SRC "Source/*.cpp")
file(GLOB_RECURSE REALITY_INC "Source/*.h")

add_executable(Reality ${REALITY_SRC} ${REALITY_INC})

target_include_directories(Reality PRIVATE
    "E:/Games/The Matrix Online/boost/boost_1_84_0"
    "../Dependencies10/CryptoPP"
    "../Dependencies10/Sockets/Project.net/Include"
    "../Dependencies10/Sockets"
    "../Dependencies10/MySQL/include"
    "Source"
)

target_link_directories(Reality PRIVATE
    "../Dependencies10/MySQL/lib/64"
)

target_link_libraries(Reality PRIVATE
    Sockets
    CryptoPP
    libmysql
    ws2_32
    Dbghelp
    Winmm
)

if(MSVC)
    target_compile_options(Reality PRIVATE /W3 /MP /bigobj)
    # Statically link runtime (like old MSBuild did with MultiThreaded /MT)
    set_property(TARGET Reality PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_target_properties(Reality PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    target_compile_options(Reality PRIVATE -Wall -O3)
    set_target_properties(Reality PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set_target_properties(Reality PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries")
