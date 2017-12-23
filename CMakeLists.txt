cmake_minimum_required (VERSION 2.6) 
project (PHYLOGENIC_LOADER)

#set(CMAKE_BUILD_TYPE DEBUG)

set (CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb -O0")
set (CMAKE_CC_FLAGS_DEBUG "-g3 -ggdb -O0")
set (CMAKE_CXX_FLAGS_RELEASE "-O3")
set (CMAKE_CC_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE MATCHES RELEASE)
	#set (CMAKE_SHARED_LIBRARY_CXX_FLAGS)
	#set (CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
	#set (CMAKE_CC_FLAGS "-static -pipe")
	#set (CMAKE_CXX_FLAGS "-static -pipe")
	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -Wl,-O1,--sort-common,--hash-style=gnu,--as-needed,-z,relro")
	#set (CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
	set (Boost_USE_STATIC_LIBS 1)
	#set (LINK_SEARCH_END_STATIC ON)
else(CMAKE_BUILD_TYPE MATCHES RELEASE)
	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
	#add_definitions("-DDEBUG")
endif(CMAKE_BUILD_TYPE MATCHES RELEASE)

find_package(Boost)
include_directories(${Boost_INCLUDE_DIR})

set(phylogeny_sources src/PhylogeneticLoader.cpp src/Taxon.cpp src/ThreadPool.cpp src/CPUTime.cpp src/Timer.cpp)
set(conv_sources src/ConvertFASTA.cpp)


add_executable(fasta_converter ${conv_sources})
add_executable(phylogeny ${phylogeny_sources})
target_link_libraries(phylogeny ${Boost_LIBRARIES})
target_link_libraries(phylogeny stdc++fs)
