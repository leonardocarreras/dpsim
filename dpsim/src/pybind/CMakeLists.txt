# SPDX-FileCopyrightText: 2021-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

set(DPSIMPY_SOURCES
	DPComponents.cpp
	EMTComponents.cpp
	SPComponents.cpp
	SignalComponents.cpp
	BaseComponents.cpp
	main.cpp
	Utils.cpp
	Attributes.cpp
)

pybind11_add_module(dpsimpy ${DPSIMPY_SOURCES})
set_target_properties(dpsimpy
	PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(dpsimpy PRIVATE dpsim)
target_include_directories(dpsimpy PUBLIC ${DPSIMPY_INCLUDE_DIRS})

install(TARGETS dpsimpy
	EXPORT dpsimpy
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_PYTHON_SITEPACKAGESDIR}
)
