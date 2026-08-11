// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <DPsim.h>
#include <iomanip>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename T>
py::cpp_function createAttributeSetter(const std::string name) {
  return [name](CPS::IdentifiedObject &object, T &value) {
    object.attributeTyped<T>(name)->set(value);
  };
}

template <typename T>
py::cpp_function createAttributeGetter(const std::string name) {
  return [name](CPS::IdentifiedObject &object) {
    return object.attributeTyped<T>(name)->get();
  };
}

CPS::Matrix zeroMatrix(int dim);

std::string getAttributeList(CPS::IdentifiedObject &obj);
void printAttributes(CPS::IdentifiedObject &obj);
void printAttribute(CPS::IdentifiedObject &obj, std::string attr);
