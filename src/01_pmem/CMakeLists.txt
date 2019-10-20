add_executable( 01_pmem main.cpp )
target_link_libraries( 01_pmem ${PMEM_LIBRARIES}
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
