
FILE(GLOB_RECURSE UtilsSrc
	*.cpp
	*.h
)

FILE(GLOB_RECURSE ProtoSrc
	*.proto
)

set( PROTO_SRCS ${Project_ROOT} )
set( PROTO_HDRS ${Project_ROOT} )

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${ProtoSrc})
SET_SOURCE_FILES_PROPERTIES(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)

#message( "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR} )

add_library( utils STATIC ${UtilsSrc} ${PROTO_SRCS} ${PROTO_HDRS} ${ProtoSrc} )

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(utils PUBLIC ${Protobuf_INCLUDE_DIRS})
target_link_libraries(utils INTERFACE ${Protobuf_LIBRARIES})
target_precompile_headers( utils PRIVATE stdafx.h )

#set_target_properties( utils PROPERTIES )
