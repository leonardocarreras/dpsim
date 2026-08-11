// SPDX-FileCopyrightText: 2022-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim/DenseLUAdapter.h>

using namespace DPsim;

namespace DPsim {

DenseLUAdapter::~DenseLUAdapter() = default;

void DenseLUAdapter::preprocessing(
    SparseMatrix &systemMatrix,
    std::vector<std::pair<UInt, UInt>> &listVariableSystemMatrixEntries) {
  /* No preprocessing phase needed by PartialPivLU */
}

void DenseLUAdapter::factorize(SparseMatrix &systemMatrix) {
  LUFactorized.compute(Matrix(systemMatrix));
}

void DenseLUAdapter::refactorize(SparseMatrix &systemMatrix) {
  /* only a simple dense factorization */
  LUFactorized.compute(Matrix(systemMatrix));
}

void DenseLUAdapter::partialRefactorize(
    SparseMatrix &systemMatrix,
    std::vector<std::pair<UInt, UInt>> &listVariableSystemMatrixEntries) {
  /* only a simple dense factorization */
  LUFactorized.compute(Matrix(systemMatrix));
}

Matrix DenseLUAdapter::solve(Matrix &mRightHandSideVector) {
  return LUFactorized.solve(mRightHandSideVector);
}
} // namespace DPsim
