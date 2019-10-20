add_executable( 00_get_physical_address main.cpp )
target_link_libraries( 00_get_physical_address
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
