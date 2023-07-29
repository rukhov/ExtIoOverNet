
FILE(GLOB_RECURSE ClientSrc
	*.cpp
	*.h
	*.def
)

add_library( net_client SHARED ${ClientSrc} )

target_precompile_headers( net_client PRIVATE stdafx.h )

target_link_libraries( net_client utils ${Protobuf_LIBRARIES} )

set_target_properties( net_client PROPERTIES
	OUTPUT_NAME "ExtIO_OverNetClient" 
	LINK_FLAGS "/SUBSYSTEM:WINDOWS"
	VS_DEBUGGER_COMMAND "C:/Program Files (x86)/SDRplay/SDRuno/SDRuno.EXTIO.exe"
)

