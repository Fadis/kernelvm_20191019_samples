cmake_minimum_required(VERSION 3.14)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
PROJECT(pmdk_examples)
include(CheckIncludeFiles)
include(FindPkgConfig)
include(CheckCXXSourceCompiles)
set(CMAKE_CXX_FLAGS_DEBUG "-std=c++17 -g -Wall -Wextra -Werror -pthread")
set(CMAKE_C_FLAGS_DEBUG "-std=c11 -g -Wall -Wextra -Werror -pthread")
set(CMAKE_CXX_FLAGS_RELEASE "-march=native -std=c++17 -W -Wall -Wextra -Werror -O3 -pthread")
set(CMAKE_C_FLAGS_RELEASE "-march=native -std=c11 -W -Wall -Wextra -Werror -O3 -pthread")
set(CMAKE_VERBOSE_MAKEFILE OFF)
find_package(PkgConfig)
find_package(Threads REQUIRED)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.65.0 COMPONENTS filesystem program_options system REQUIRED )
pkg_check_modules(PMEM REQUIRED libpmem)
pkg_check_modules(PMEMOBJ REQUIRED libpmemobj)
pkg_check_modules(PMEMLOG REQUIRED libpmemlog)
pkg_check_modules(PMEMBLK REQUIRED libpmemblk)
INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}/include ${Boost_INCLUDE_DIRS}
	${PMEM_INCLUDE_DIR} ${PMEMOBJ_INCLUDE_DIRS} ${PMEMLOG_INCLUDE_DIRS}
	${PMEMBLK_INCLUDE_DIRS} )
link_directories( ${Boost_LIBRARY_DIRS} ${PMEM_LIBRARY_DIRS}
	${PMEMOBJ_LIBRARY_DIRS} ${PMEMLOG_LIBRARY_DIRS} ${PMEMBLK_LIBRARY_DIRS} )
subdirs( src )

