cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 6)
set(CMAKE_C_STANDARD 23)
set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x86")

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

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message("Debug config")
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CLANG_TIME_TRACE)
		if (CLANG_TIME_TRACE)
			message("Clang time trace enabled!")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftime-trace")
		endif()

		# Adress sanitizer is unsuported on windows
		if (NOT WIN32)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak -fsanitize=address -fsanitize=thread")
		endif()

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wextra")
	endif()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -v")
else()
	message("Release config")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 ")
	# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	# set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	#set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	#add_compile_options("-fsanitize=address")
	#link_libraries("-fsanitize=address")
	#add_definitions(-fsanitize=address -g3 -fno-omit-frame-pointer)

	message(STATUS "Compiling with Address Sanitizer (ASAN) support")
    add_definitions(-fsanitize=address -fno-omit-frame-pointer -g -fno-optimize-sibling-calls -O1)
    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

	# set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
	# set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fsanitize=address -fno-omit-frame-pointer")

	#if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
	#endif()
endif()

# Atomics are still under experimental...
if (MSVC)
	add_compile_options(/experimental:c11atomics /wd5040)
endif()

find_package(Threads REQUIRED)

set(TINYCTHREAD_DISABLE_TESTS ON)
add_subdirectory(libraries/tinycthread)

add_executable(ThreadPoolExp ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(ThreadPoolExp PROPERTIES OUTPUT_NAME "ThreadPoolExp")

include_directories(${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(ThreadPoolExp Threads::Threads tinycthread)

# Parallel build
add_custom_target(	my_parallel_build 
                    COMMAND ${CMAKE_COMMAND} --build -j ${CMAKE_BUILD_PARALLEL_LEVEL}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}	)