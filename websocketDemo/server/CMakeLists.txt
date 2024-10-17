cmake_minimum_required(VERSION 3.10)
project(websocket_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# Add WebRTC
set(WEBRTC_ROOT "C:/webrtc") # Adjust this path to your WebRTC installation
set(WEBRTC_INCLUDE_DIRS "${WEBRTC_ROOT}/include")
set(WEBRTC_LIBRARIES "${WEBRTC_ROOT}/lib/webrtc.lib") # Adjust the library name if needed

# Add Abseil (part of WebRTC)
set(ABSL_INCLUDE_DIRS "${WEBRTC_ROOT}/include/third_party/abseil-cpp")

# Add the executable
add_executable(server server.cpp)

# Include directories
target_include_directories(server PRIVATE 
    ${Boost_INCLUDE_DIRS}
    C:/GitRepo/websocketpp/install/include
    ${WEBRTC_INCLUDE_DIRS}
    ${ABSL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(server PRIVATE
    ${Boost_LIBRARIES}
    Threads::Threads
    ${WEBRTC_LIBRARIES}
    winmm.lib
    Secur32.lib
)

# If websocketpp is not installed system-wide, you may need to specify its path
# For example:
# target_include_directories(server PRIVATE /path/to/websocketpp)

# Set compiler flags and runtime library
if(MSVC)
    target_compile_options(server PRIVATE /W4 /MT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add WebRTC-specific compiler definitions
target_compile_definitions(server PRIVATE
    WEBRTC_WIN
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# Add additional Windows libraries
target_link_libraries(server PRIVATE
    Iphlpapi.lib
    Msdmo.lib
    Dmoguids.lib
    wmcodecdspuuid.lib
)
