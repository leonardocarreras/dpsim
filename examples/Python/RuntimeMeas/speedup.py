#!/usr/bin/python3

# SPDX-FileCopyrightText: 2019-2022 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

import csv
import sys

from meas_utils import *

if len(sys.argv) != 4:
    sys.exit("usage: speedup.py old.csv new.csv outname")

old = Measurement.read_csv(sys.argv[1])
new = Measurement.read_csv(sys.argv[2])

meas = new.speedup(old, sys.argv[3])
meas.save()
