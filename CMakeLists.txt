cmake_minimum_required(VERSION 3.0)
project(asd CXX)

###################################
# external dependencies

include(FindPkgConfig)

find_package(Boost REQUIRED COMPONENTS thread;program_options;chrono)

SET (CMAKE_CXX_FLAGS "-DMQTT_NO_TLS ${CMAKE_CXX_FLAGS}")

#####################
# build paths

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/third-party/mqtt_cpp/include
)
# ${PROJECT_SOURCE_DIR}/externals/mqtt_cpp/include

link_directories(
    ${LIBSSL_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIRS}
)

######################
# server target

add_executable(maxcube2mqtt
src/main.cpp
src/cube_io.cpp
src/cube.cpp
src/utils.cpp
src/cube_log.cpp
src/cube_mqtt_client.cpp
src/io_operator.cpp
)

set_property(TARGET maxcube2mqtt PROPERTY CXX_STANDARD 17)

target_link_libraries(maxcube2mqtt
    ${Boost_LIBRARIES}
    pthread
    )
