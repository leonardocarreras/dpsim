# SPDX-FileCopyrightText: 2022-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

from . import matpower
from .matpower import Reader

try:
    from dpsimpy import *
except ImportError:  # pragma: no cover
    print("Error: Could not find dpsim C++ module.")

__all__ = ["matpower"]
