# SPDX-FileCopyrightText: 2017-2023 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

find_program(SPHINX_EXECUTABLE NAMES sphinx-build
	HINTS
	$ENV{SPHINX_DIR}
	PATH_SUFFIXES bin
	DOC "Sphinx documentation generator"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sphinx DEFAULT_MSG SPHINX_EXECUTABLE)

mark_as_advanced(SPHINX_EXECUTABLE)
