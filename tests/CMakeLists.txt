cmake_minimum_required(VERSION 2.6.0)

project(RunTests)

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
  DEPENDS ${CMAKE_SOURCE_DIR}/../VERSION
  COMMAND ${CMAKE_SOURCE_DIR}/../mkversion ${CMAKE_SOURCE_DIR}/..
  COMMENT "Generating version header"
  VERBATIM
)
include_directories(${CMAKE_BINARY_DIR})

# Set plugin type (south, north, filter)
set(PLUGIN_TYPE "south")

# Add here all needed FogLAMP libraries as list
set(NEEDED_FOGLAMP_LIBS common-lib services-common-lib)

set(BOOST_COMPONENTS system thread)

find_package(Boost 1.53.0 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

# Find source files
file(GLOB SOURCES ../*.cpp)
file(GLOB unittests "*.cpp")

# Find FogLAMP includes and libs, by including FindFogLAMP.cmak file
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/..)
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


# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add ../include
include_directories(../include)
# Add FogLAMP include dir(s)
include_directories(${FOGLAMP_INCLUDE_DIRS})

# Add other include paths
if (FOGLAMP_SRC)
	message(STATUS "Using third-party includes " ${FOGLAMP_SRC}/C/thirdparty)
	include_directories(${FOGLAMP_SRC}/C/thirdparty/rapidjson/include)
	include_directories(${FOGLAMP_SRC}/C/thirdparty/Simple-Web-Server)
endif()

# Add FogLAMP lib path
link_directories(${FOGLAMP_LIB_DIRS})

# Find python3.x dev/lib package
find_package(PkgConfig REQUIRED)
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    pkg_check_modules(PYTHON REQUIRED python3)
else()
    find_package(Python COMPONENTS Interpreter Development)
endif()

# Add Python 3.x header files
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    include_directories(${PYTHON_INCLUDE_DIRS})
else()
    include_directories(${Python_INCLUDE_DIRS})
endif()

# Add additional link directories
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    link_directories(${PYTHON_LIBRARY_DIRS})
else()
    link_directories(${Python_LIBRARY_DIRS})
endif()


# Link runTests with what we want to test and the GTest and pthread library
add_executable(RunTests ${unittests} ${SOURCES} version.h)

# Add additional libraries
if(${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    target_link_libraries(${PROJECT_NAME} -lssl -lcrypto -lpaho-mqtt3cs ${PYTHON_LIBRARIES})
else()
    target_link_libraries(${PROJECT_NAME} -lssl -lcrypto -lpaho-mqtt3cs ${Python_LIBRARIES})
endif()

set(FOGLAMP_INSTALL "" CACHE INTERNAL "")
# Install library
if (FOGLAMP_INSTALL)
	message(STATUS "Installing ${PROJECT_NAME} in ${FOGLAMP_INSTALL}/plugins/${PLUGIN_TYPE}/${PROJECT_NAME}")
	install(TARGETS ${PROJECT_NAME} DESTINATION ${FOGLAMP_INSTALL}/plugins/${PLUGIN_TYPE}/${PROJECT_NAME})
endif()


target_link_libraries(RunTests ${GTEST_LIBRARIES} pthread)
target_link_libraries(RunTests ${NEEDED_FOGLAMP_LIBS})
target_link_libraries(RunTests  ${Boost_LIBRARIES})
target_link_libraries(RunTests -lpthread -ldl)
