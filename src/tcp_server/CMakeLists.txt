
FILE(GLOB_RECURSE SrvSrc
	*.cpp
	*.h
)

add_executable( tcp_server ${SrvSrc} )

target_precompile_headers( tcp_server PRIVATE stdafx.h )

target_link_libraries( tcp_server utils )

set_target_properties( tcp_server PROPERTIES 
	OUTPUT_NAME "ExtIO_OverNetServer"
	LINK_FLAGS "/SUBSYSTEM:CONSOLE"
	VS_DEBUGGER_COMMAND_ARGUMENTS "--log_level=0 --listening_port=2056 --extio_path=C:/usr/projects/tpc/ExtIO_SDRplay/RSP1A/Debug/ExtIO_SDRplay_RSP1A.dll"
)

