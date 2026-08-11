# SPDX-FileCopyrightText: 2020-2023 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

set(SIGNALS_SRCS
	FIRFilter.cpp
	TurbineGovernor.cpp
)

foreach(SOURCE ${SIGNALS_SRCS})
	get_filename_component(TARGET ${SOURCE} NAME_WE)
	add_executable(${TARGET} ${SOURCE})
	target_link_libraries(${TARGET} dpsim-models)
endforeach()
