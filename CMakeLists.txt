cmake_minimum_required(VERSION 3.28)
project(tmsmt_core LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(tmsmt_core SHARED SingleServer.cpp)

set_target_properties(tmsmt_core PROPERTIES OUTPUT_NAME "tmsmt_core")

# Find Boost
find_package(Boost REQUIRED)

# If you need specific Boost components, list them like so:
# find_package(Boost REQUIRED COMPONENTS filesystem system)

# Include Boost's headers
include_directories(${Boost_INCLUDE_DIRS})

# Check if we are on Windows and if so, link against ws2_32
if(WIN32)
    target_link_libraries(tmsmt_core PRIVATE ws2_32)
endif()
