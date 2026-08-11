# SPDX-FileCopyrightText: 2022-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

include(FetchContent)

FetchContent_Declare(
	suitesparse-module
	URL https://github.com/dpsim-simulator/SuiteSparse/releases/download/release-v5.10.6/SuiteSparse-release-v5.10.6.tar.gz
)

FetchContent_MakeAvailable(suitesparse-module)

add_library(SuiteSparse::KLU ALIAS klu)

set(SuiteSparse_FOUND ON)
