# SPDX-FileCopyrightText: 2023-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

option(SPDLOG_BUILD_TESTING "Build spdlog tests" OFF)
option(SPDLOG_BUILD_BENCH "Build spdlog benchmarks" OFF)
option(SPDLOG_BUILD_EXAMPLES "Build spdlog examples" OFF)

include(FetchContent)
FetchContent_Declare(spdlog-module
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG        v1.15.0
	GIT_SHALLOW    TRUE
	GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(spdlog-module)
