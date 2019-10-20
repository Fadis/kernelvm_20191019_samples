add_executable( 03_pmemobj_alloc main.cpp )
target_link_libraries( 03_pmemobj_alloc ${PMEM_LIBRARIES} ${PMEMOBJ_LIBRARIES}
	${Boost_PROGRAM_OPTIONS_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${Boost_SYSTEM_LIBRARIES} )
