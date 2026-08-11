// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/Definitions.h>
#include <vector>

namespace CPS {
class ODEintInterface {
public:
  typedef std::shared_ptr<ODEintInterface> Ptr;
  using stateFnc = std::function<void(const double *, double *, const double)>;

  // #### ODE Section ####
  /// Returns number of differential variables
  virtual int num_states() const = 0;
  /// Sets up ODE system in ydot
  virtual void odeint(const double y[], double ydot[], double t) = 0;

  /// Needed for computations which have to be carried out before the numerical approximation step
  virtual void pre_step() = 0;
  /// Writes the values from the constructed state vector back into the original fields
  virtual void post_step() = 0;
  ///Returns Pointer to state Vector of the componente
  virtual double *state_vector() = 0;
  ///Writes the computed solution to the component
  virtual void set_state_vector(std::vector<double> y) = 0;
};
} // namespace CPS
