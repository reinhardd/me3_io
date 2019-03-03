cmake_minimum_required(VERSION 3.0)
project(asd CXX)

###################################
# external dependencies

include(FindPkgConfig)

find_package(Boost REQUIRED)

#####################
# build paths

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/externals/mqtt_cpp/include
)

link_directories(
    ${LIBSSL_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIRS}
)

######################
# server target

add_executable(max2mqtt
src/main.cpp
src/cube_io.cpp
src/cube.cpp
src/utils.cpp
src/cube_log.cpp
)

set_property(TARGET max2mqtt PROPERTY CXX_STANDARD 17)

target_link_libraries(max2mqtt
    ${Boost_LIBRARIES}
    pthread
    )

