add_executable( 06_pmemblk main.cpp )
target_link_libraries( 06_pmemblk ${PMEM_LIBRARIES} ${PMEMBLK_LIBRARIES}
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
