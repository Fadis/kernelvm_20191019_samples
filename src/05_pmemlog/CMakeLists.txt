add_executable( 05_pmemlog main.cpp )
target_link_libraries( 05_pmemlog ${PMEM_LIBRARIES} ${PMEMLOG_LIBRARIES}
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
