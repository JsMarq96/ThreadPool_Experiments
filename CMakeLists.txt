cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 6)
set(CMAKE_C_STANDARD 23)
set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x86")

option(CLANG_TIME_TRACE "Enable clang profiling." OFF)
option(CLANG_ASAN "Enable Adress Sanitizer." OFF)
option(CLANG_ASAN_RELEASE "Force Adress Sanitizer on Release." OFF)

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

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "Debug config")
	
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		if (CLANG_TIME_TRACE)
			message(STATUS "Clang time trace enabled!")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftime-trace")
		endif()

		if(CLANG_ASAN)
			message(STATUS "Compiling with Address Sanitizer")
			add_definitions(-fsanitize=address -fno-omit-frame-pointer -g -fno-optimize-sibling-calls)
			set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
		endif()

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
	endif()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -v -pedantic")
else()
	message(STATUS "Release config")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

	if(CLANG_ASAN_RELEASE)
		message(STATUS "Compiling with Address Sanitizer")
		add_definitions(-fsanitize=address -fno-omit-frame-pointer -g -fno-optimize-sibling-calls)
		set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	endif()
endif()

# Atomics are still under experimental...
if (MSVC)
	add_compile_options(/experimental:c11atomics /wd5040)
endif()

set(TINYCTHREAD_DISABLE_TESTS ON)
add_subdirectory(libraries/tinycthread)

add_executable(ThreadPoolExp ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(ThreadPoolExp PROPERTIES OUTPUT_NAME "ThreadPoolExp")

include_directories(${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(ThreadPoolExp tinycthread)

# Parallel build
add_custom_target(	my_parallel_build 
                    COMMAND ${CMAKE_COMMAND} --build -j ${CMAKE_BUILD_PARALLEL_LEVEL}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}	)