#!/bin/bash

# SPDX-FileCopyrightText: 2019-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

CPS_LOG_PREFIX="[Left ] " build/dpsim-villas/examples/cxx/ShmemDistributedDirect 0 & P1=$!
CPS_LOG_PREFIX="[Right] " build/dpsim-villas/examples/cxx/ShmemDistributedDirect 1 & P2=$!

for job in $P1 $P2; do
    wait $job || exit 1
done
