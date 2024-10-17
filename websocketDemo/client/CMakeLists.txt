cmake_minimum_required(VERSION 3.10)
project(websocket_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set MSVC runtime library to static for all configurations
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Configure Boost settings before finding the package
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)  # Change this to OFF to use shared runtime
set(Boost_USE_MULTITHREADED ON)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system random)
find_package(Threads REQUIRED)

# Add WebRTC
set(WEBRTC_ROOT "C:/webrtc") # Adjust this path to your WebRTC installation
set(WEBRTC_INCLUDE_DIRS "${WEBRTC_ROOT}/include")
set(WEBRTC_LIBRARIES 
    "${WEBRTC_ROOT}/lib/webrtc.lib"
)

# Add Abseil (part of WebRTC)
set(ABSL_INCLUDE_DIRS "${WEBRTC_ROOT}/include/third_party/abseil-cpp")

# Add the executable
add_executable(client client.cpp)

# Include directories
target_include_directories(client PRIVATE 
    ${Boost_INCLUDE_DIRS}
    C:/GitRepo/websocketpp/install/include
    ${WEBRTC_INCLUDE_DIRS}
    ${ABSL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(client PRIVATE
    ${Boost_LIBRARIES}
    Threads::Threads
    ${WEBRTC_LIBRARIES}
    ws2_32
    winmm
    secur32
    iphlpapi
    dmoguids
    msdmo
    wmcodecdspuuid
)

# If websocketpp is not installed system-wide, you may need to specify its path
# For example:
# target_include_directories(client PRIVATE /path/to/websocketpp)

# Set compiler flags
if(MSVC)
    target_compile_options(client PRIVATE /W4 /wd4100 /wd4996 /MT$<$<CONFIG:Debug>:d>)
else()
    target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add WebRTC-specific compiler definitions
target_compile_definitions(client PRIVATE
    WEBRTC_WIN
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WEBRTC_ENABLE_PROTOBUF=0
    WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE
)

# Ensure Boost is built with static runtime
if(MSVC)
    add_definitions(-DBOOST_ALL_NO_LIB)
    add_definitions(-DBOOST_ALL_STATIC_LINK)
endif()

# Explicitly set Boost library paths if needed
if(MSVC)
    set(BOOST_ROOT "C:/Program Files/Boost" CACHE PATH "Boost root directory")
    set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib" CACHE PATH "Boost library directory")
    set(Boost_SYSTEM_LIBRARY_RELEASE "${BOOST_LIBRARYDIR}/libboost_system-vc143-mt-x64-1_86.lib" CACHE FILEPATH "Path to Boost System library (release)")
    set(Boost_SYSTEM_LIBRARY_DEBUG "${BOOST_LIBRARYDIR}/libboost_system-vc143-mt-gd-x64-1_86.lib" CACHE FILEPATH "Path to Boost System library (debug)")
    set(Boost_RANDOM_LIBRARY_RELEASE "${BOOST_LIBRARYDIR}/libboost_random-vc143-mt-x64-1_86.lib" CACHE FILEPATH "Path to Boost Random library (release)")
    set(Boost_RANDOM_LIBRARY_DEBUG "${BOOST_LIBRARYDIR}/libboost_random-vc143-mt-gd-x64-1_86.lib" CACHE FILEPATH "Path to Boost Random library (debug)")
endif()
