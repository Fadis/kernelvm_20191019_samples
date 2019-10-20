add_executable( 00_mmap main.cpp )
target_link_libraries( 00_mmap
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
