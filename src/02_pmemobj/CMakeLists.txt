add_executable( 02_pmemobj main.cpp )
target_link_libraries( 02_pmemobj ${PMEM_LIBRARIES} ${PMEMOBJ_LIBRARIES}
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
