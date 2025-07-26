cmake_minimum_required(VERSION 3.10)
project(xrtc_rtmppush)

set(CMAKE_CXX_STANDARD 11)

include_directories(${CMAKE_SOURCE_DIR}/src)

file(GLOB SRC_FILES
    src/*.cpp
)

add_executable(xrtc_rtmppush ${SRC_FILES})

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(xrtc_rtmppush
    ${CURL_LIBRARIES}
    Threads::Threads
)

# 如果需要其他库，可以在这里添加
# target_link_libraries(xrtc_rtmppush <other_library>)