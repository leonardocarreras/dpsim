# SPDX-FileCopyrightText: 2020-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

if(WITH_GRAPHVIZ AND WITH_CIM)
	add_executable(cimviz cimviz.cpp)
	target_link_libraries(cimviz PUBLIC dpsim-models CIMPP::cimpp ${GRAPHVIZ_LIBRARIES})
endif()
