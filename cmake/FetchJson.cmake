# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

include(FetchContent)
FetchContent_Declare(json
	GIT_REPOSITORY https://github.com/nlohmann/json
	GIT_TAG        v3.11.3
	GIT_SHALLOW    TRUE
	GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(json)

set(nlohmann_json_FOUND ON)
