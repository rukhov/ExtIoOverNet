cmake_minimum_required(VERSION 3.26)

project(extio2tcp VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)

if (MSVC)
    add_compile_options(/experimental:module-)
endif()

if( MSVC_VERSION GREATER_EQUAL 1936 AND MSVC_IDE ) # 17.6+
  # When using /std:c++latest, "Build ISO C++23 Standard Library Modules" defaults to "Yes".
  # Default to "No" instead.
  #
  # As of CMake 3.26.4, there isn't a way to control this property
  # (https://gitlab.kitware.com/cmake/cmake/-/issues/24922),
  # We'll use the MSBuild project system instead
  # (https://learn.microsoft.com/en-us/cpp/build/reference/vcxproj-file-structure)
  file( CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/Directory.Build.props" CONTENT [==[
<Project>
  <ItemDefinitionGroup>
    <ClCompile>
      <BuildStlModules>false</BuildStlModules>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
]==] @ONLY )
endif()


# ==========================================================================
# Protocol bufers section

#set( PROTOBUF_ROOT ${Protobuf_INSTALL_DIR})
#set( Protobuf_INCLUDE_DIR ${PROTOBUF_ROOT}/include )
#set( Protobuf_LIBRARY_DEBUG ${PROTOBUF_ROOT}/lib/libprotobufd.lib )
#set( Protobuf_LITE_LIBRARY_DEBUG ${PROTOBUF_ROOT}/Debug/libprotobuf-lited.lib )

#message( "Protobuf_INCLUDE_DIR: " ${Protobuf_INCLUDE_DIR} )
#message( "CMAKE_BINARY_DIR: " ${CMAKE_BINARY_DIR})
#message( "Protobuf_LITE_LIBRARY_DEBUG: " ${Protobuf_LITE_LIBRARY_DEBUG})

include(FindProtobuf)
#find_package(Protobuf REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

find_package(Boost
#  1.84.0
  #[EXACT]             # Minimum or EXACT version e.g. 1.67.0
  REQUIRED             # Fail with error if Boost is not found
  #[COMPONENTS <libs>...] # Boost libraries by their canonical name
                         # e.g. "date_time" for "libboost_date_time"
  #[OPTIONAL_COMPONENTS <libs>...]
                         # Optional Boost libraries by their canonical name)
  )

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

#include_directories( ${Boost_INCLUDE_DIRS} )
#link_directories( ${Boost_LIBRARY_DIRS} )

add_subdirectory( src )

#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT tcp_server)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT net_client)
