cmake_minimum_required(VERSION 3.25)
project(SimpleNetworking)

set(CMAKE_CXX_STANDARD 20)

set(BASEDIR SimpleNetworking)
set(INCLUDEDIR ${BASEDIR}/include)
set(SOURCEDIR ${BASEDIR}/src)
set(EXTERNALDIR ${BASEDIR}/external)

set(external_folders
        ${EXTERNALDIR}/catch
)

set(header_files 

        ${INCLUDEDIR}/ISocket.h
        ${INCLUDEDIR}/SerialPort.h
        ${INCLUDEDIR}/TCPSocket.h
        ${INCLUDEDIR}/UDPSocket.h

)

set(source_files

        ${SOURCEDIR}/SocketTests.cpp
)

include_directories(${external_folders})

find_package(Boost REQUIRED)

if(Boost_FOUND)
include_directories(${Boost_INCLUDE_DIRS})
endif()

add_executable(SimpleNetworking ${source_files} ${header_files})
