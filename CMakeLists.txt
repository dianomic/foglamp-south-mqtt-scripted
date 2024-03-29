cmake_minimum_required(VERSION 2.6.0)

# Set the plugin name to build
project(mqtt-scripted)

# Supported options:
# -DFOGLAMP_INCLUDE
# -DFOGLAMP_LIB
# -DFOGLAMP_SRC
# -DFOGLAMP_INSTALL
#
# If no -D options are given and FOGLAMP_ROOT environment variable is set
# then FogLAMP libraries and header files are pulled from FOGLAMP_ROOT path.

set(CMAKE_CXX_FLAGS "-std=c++11 -O3")

# Generation version header file
set_source_files_properties(version.h PROPERTIES GENERATED TRUE)
add_custom_command(
  OUTPUT version.h
  DEPENDS ${CMAKE_SOURCE_DIR}/VERSION
  COMMAND ${CMAKE_SOURCE_DIR}/mkversion ${CMAKE_SOURCE_DIR}
  COMMENT "Generating version header"
  VERBATIM
)
include_directories(${CMAKE_BINARY_DIR})

# Set plugin type (south, north, filter)
set(PLUGIN_TYPE "south")

# Add here all needed FogLAMP libraries as list
set(NEEDED_FOGLAMP_LIBS common-lib)

# Find source files
file(GLOB SOURCES *.cpp)

# Find python3.x dev/lib package
find_package(PkgConfig REQUIRED)
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    pkg_check_modules(PYTHON REQUIRED python3)
else()
    find_package(Python COMPONENTS Interpreter Development)
endif()

# Find FogLAMP includes and libs, by including FindFogLAMP.cmak file
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})
find_package(FogLAMP)
# If errors: make clean and remove Makefile
if (NOT FOGLAMP_FOUND)
	if (EXISTS "${CMAKE_BINARY_DIR}/Makefile")
		execute_process(COMMAND make clean WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
		file(REMOVE "${CMAKE_BINARY_DIR}/Makefile")
	endif()
	# Stop the build process
	message(FATAL_ERROR "FogLAMP plugin '${PROJECT_NAME}' build error.")
endif()
# On success, FOGLAMP_INCLUDE_DIRS and FOGLAMP_LIB_DIRS variables are set 

# Add ./include
include_directories(include)
# Add FogLAMP include dir(s)
include_directories(${FOGLAMP_INCLUDE_DIRS})

# Add Python 3.x header files
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    include_directories(${PYTHON_INCLUDE_DIRS})
else()
    include_directories(${Python_INCLUDE_DIRS})
endif()

if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
	set(PY_LIB "lib${PYTHON_LIBRARIES}.so")
else()
	set(PY_LIB "${Python_LIBRARIES}")
endif()
add_definitions(-DPLUGIN_PYTHON_SHARED_LIBRARY=${PY_LIB})
message(STATUS "Setting -DPYTHON_SHARED_LIBRARY=${PY_LIB} for ${CMAKE_SYSTEM} on ${RELEASE_CODENAME}")

if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    link_directories(${PYTHON_LIBRARY_DIRS})
else()
    link_directories(${Python_LIBRARY_DIRS})
endif()


# Add other include paths

# Add FogLAMP lib path
link_directories(${FOGLAMP_LIB_DIRS})


# Create shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES} version.h)

# Add FogLAMP library names
target_link_libraries(${PROJECT_NAME} ${NEEDED_FOGLAMP_LIBS})

# Add additional libraries
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    target_link_libraries(${PROJECT_NAME} -lssl -lcrypto -lpaho-mqtt3cs ${PYTHON_LIBRARIES})
else()
    target_link_libraries(${PROJECT_NAME} -lssl -lcrypto -lpaho-mqtt3cs ${Python_LIBRARIES})
endif()

# Set the build version 
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION 1)

set(FOGLAMP_INSTALL "" CACHE INTERNAL "")
# Install library
if (FOGLAMP_INSTALL)
	message(STATUS "Installing ${PROJECT_NAME} in ${FOGLAMP_INSTALL}/plugins/${PLUGIN_TYPE}/${PROJECT_NAME}")
	install(TARGETS ${PROJECT_NAME} DESTINATION ${FOGLAMP_INSTALL}/plugins/${PLUGIN_TYPE}/${PROJECT_NAME})
endif()

