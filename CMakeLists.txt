cmake_minimum_required(VERSION 3.20min)

include ("project_configs.cmake")

project (${PROJECT_NAME})

include_directories ("${PROJECT_SOURCE_DIR}/include")
include_directories ("${PROJECT_SOURCE_DIR}/third_party")

if (${MSVC_PROJECT})
	file (GLOB_RECURSE file_list
		LIST_DIRECTORIES false
		"${PROJECT_SOURCE_DIR}/include/*.h"
		"${PROJECT_SOURCE_DIR}/include/*.inl"
		"${PROJECT_SOURCE_DIR}/src/*.cpp")
else ()
	set (file_list
		"${PROJECT_SOURCE_DIR}/src/profiler.cpp")
endif (${MSVC_PROJECT})

add_definitions(
	-D_CRT_SECURE_NO_WARNINGS)

if (${ANDROID_BUILD})
	message(STATUS ${PROJECT_NAME} " will be built using Android configs")
	add_definitions (
		-DPLATFORM_POSIX)

	# platform abi
	if (${ANDROID_ABI} STREQUAL "arm64-v8a")
		message(STATUS ${PROJECT_NAME} " Android ABI: arm64")
		add_definitions (
			-DPOSIX64)
	else ()
		message(STATUS ${PROJECT_NAME} " Android ABI: arm")
		add_definitions (
			-DPOSIX32)
	endif (${ANDROID_ABI} STREQUAL "arm64-v8a")
	
	include_directories ("${PROJECT_SOURCE_DIR}/third_party/hwcpipe")
	
	file (GLOB_RECURSE hwcpipe_file_list
		LIST_DIRECTORIES false
		"${PROJECT_SOURCE_DIR}/third_party/hwcpipe/*.cpp")
	set (file_list ${file_list} ${hwcpipe_file_list})
	
else ()
	message(STATUS ${PROJECT_NAME} " will be built using Windows configs")
	add_definitions (
		-DPLATFORM_WINDOWS)
endif (${ANDROID_BUILD})

add_library (${PROJECT_NAME} ${file_list})

target_link_libraries(${PROJECT_NAME}
	floral
	helich)

set (include_dir_list
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/third_party")

target_include_directories (${PROJECT_NAME} PUBLIC
	"$<BUILD_INTERFACE:${include_dir_list}>")

if (${MSVC_PROJECT})
	# organize filters
	foreach(_source IN ITEMS ${file_list})
		get_filename_component(_source_path "${_source}" PATH)
		file(RELATIVE_PATH _source_path_rel "${PROJECT_SOURCE_DIR}" "${_source_path}")
		string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
		source_group("${_group_path}" FILES "${_source}")
	endforeach()
endif (${MSVC_PROJECT})
