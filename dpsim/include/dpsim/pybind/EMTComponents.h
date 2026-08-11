// SPDX-FileCopyrightText: 2021-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <pybind11/complex.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;

void addEMTComponents(py::module_ mEMT);
void addEMTPh1Components(py::module_ mEMTPh1);
void addEMTPh3Components(py::module_ mEMTPh3);
