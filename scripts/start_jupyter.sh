# SPDX-FileCopyrightText: 2019-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

# helper script to start jupyter notebook in dpsim-dev container
# assumes dpsim and data-processing to be mounted in /dpsim-dev
cd /dpsim-dev/dpsim/build
export PYTHONPATH="$(pwd)/Source/Python:$(pwd)/../Source/Python"
cd /dpsim-dev/dpsim
jupyter lab --ip="0.0.0.0" --allow-root --no-browser
