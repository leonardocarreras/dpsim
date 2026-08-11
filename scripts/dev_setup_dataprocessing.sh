# SPDX-FileCopyrightText: 2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

# helper script to prepare dpsim-dev container to work with
# local dataprocessing package assumes dpsim and data-processing
# to be mounted in /dpsim-dev

pip3 uninstall villas-dataprocessing
cd /dpsim-dev/data-processing

pip3 install -e .
cd /dpsim-dev/dpsim/build

export PYTHONPATH=$(pwd)/Source/Python:$(pwd)/../Source/Python
