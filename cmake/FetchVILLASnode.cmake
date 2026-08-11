# SPDX-FileCopyrightText: 2023 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

include(FetchContent)
FetchContent_Declare(villas-node-module
	GIT_REPOSITORY https://github.com/VILLASframework/node.git
	GIT_SHALLOW    TRUE
	GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(villas-node-module)

set(VILLASnode_FOUND TRUE)
