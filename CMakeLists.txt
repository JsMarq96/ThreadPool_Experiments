cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 6)
set(CMAKE_C_STANDARD 23)

option(CLANG_TIME_TRACE "Enable clang profiling." ON)

project(ThreadPoolExp)

set(SRC_FILE_DIR "src/")

macro(ADD_SOURCE)
	file(GLOB H_FILES_APPEND "${ARGV0}/*.h")
	file(GLOB C_FILES_APPEND "${ARGV0}/*.c")

	list(APPEND HEADER_FILES ${H_FILES_APPEND})
	list(APPEND SOURCE_FILES ${C_FILES_APPEND})
endmacro()

ADD_SOURCE("src/")
#ADD_SOURCE("src/utils")

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CLANG_TIME_TRACE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftime-trace")
endif()

find_package(Threads REQUIRED)

add_subdirectory(libraries/tinycthread)

add_executable(ThreadPoolExp ${HEADER_FILES} ${SOURCE_FILES})
set_target_properties(ThreadPoolExp PROPERTIES OUTPUT_NAME "ThreadPoolExp")

include_directories(${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(ThreadPoolExp Threads::Threads tinycthread)

# Parallel build
add_custom_target(my_parallel_build 
                      COMMAND ${CMAKE_COMMAND} --build -j ${CMAKE_BUILD_PARALLEL_LEVEL}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})