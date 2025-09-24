configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/include/dpsim/Config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/dpsim/Config.h
)

set(DPSIM_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}/include
)

add_subdirectory(src)

if(WITH_PYBIND)
	set(DPSIMPY_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/Include/dpsim/pybind)
	add_subdirectory(src/pybind)
endif()

if(DPSIM_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

file(GLOB_RECURSE HEADER_FILES include/*.h)

target_sources(dpsim PUBLIC
	FILE_SET public_headers
	TYPE HEADERS
	BASE_DIRS include
	FILES "${HEADER_FILES}"
)

# Enable OpenMP on the core library (and expose a define).
if (WITH_OPENMP AND TARGET OpenMP::OpenMP_CXX)
	target_link_libraries(dpsim PUBLIC OpenMP::OpenMP_CXX)
	target_compile_definitions(dpsim PUBLIC DPSIM_WITH_OPENMP)
endif()

if (WITH_OPENMP AND TARGET OpenMP::OpenMP_CXX)
	if () # adjust to your actual pybind target name if different
		target_link_libraries(dpsimpy PRIVATE OpenMP::OpenMP_CXX)
		target_compile_definitions(dpsimpy PRIVATE DPSIM_WITH_OPENMP)
	endif()
endif()

install(TARGETS dpsim
	EXPORT dpsim
	FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
