# SPDX-FileCopyrightText: 2023 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

include(FetchContent)
FetchContent_Declare(eigen-module
	GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
	GIT_TAG        3fe8c511042ab1af84c2f91015463708f969255e
	GIT_PROGRESS   TRUE
)

set(EIGEN_BUILD_DOC OFF)

FetchContent_MakeAvailable(eigen-module)
