/* Copyright 2017-2020 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <cps/SimPowerComp.h>
#include <cps/Solver/PFSolverInterfaceBus.h>


namespace CPS {

namespace SP {
namespace Ph1 {

        class SynchronGenerator :
			public SimPowerComp<Complex>,
			public SharedFactory<SynchronGenerator>,
			public PFSolverInterfaceBus {
	    private:
			/// Rate apparent power [VA]
		    Real mRatedApparentPower;
			/// Rated voltage [V]
		    Real mRatedVoltage;

			/// Active power set point of the machine [W]
			Real mSetPointActivePower;
			/// Reactive power set point of the machine [VAr]
			Real mSetPointReactivePower;
			/// Voltage set point of the machine [V]
			Real mSetPointVoltage;
			/// Maximum reactive power [VAr]
			Real mMaximumReactivePower;

			/// Base apparent power[VA]
			Real mBaseApparentPower;
			/// Base omega [1/s]
			Real mBaseOmega;
			/// Base voltage [V]
			Real mBaseVoltage;
			/// Active power set point of the machine [pu]
			Real mSetPointActivePowerPerUnit;
			/// Reactive power set point of the machine [pu]
			Real mSetPointReactivePowerPerUnit;
			/// Voltage set point of the machine [pu]
			Real mSetPointVoltagePerUnit;


        public:
			/// Defines UID, name and logging level
			SynchronGenerator(String uid, String name, Logger::Level logLevel = Logger::Level::off);
			/// Defines name and logging level
			SynchronGenerator(String name, Logger::Level logLevel = Logger::Level::off)
				: SynchronGenerator(name, name, logLevel) { }
			/// Setter for synchronous generator parameters
			void setParameters(Real ratedApparentPower, Real ratedVoltage, Real setPointActivePower, Real setPointVoltage, PowerflowBusType powerflowBusType, Real setPointReactivepower=0);
			// #### Powerflow section ####
			/// Set base voltage
			void setBaseVoltage(Real baseVoltage);
			/// Initializes component from power flow data
			void calculatePerUnitParameters(Real baseApparentPower, Real baseOmega);
            /// Modify powerflow bus type
			void modifyPowerFlowBusType(PowerflowBusType powerflowBusType) override;
			/// Update reactive power injection (PV Bus)
			void updateReactivePowerInjection(Complex powerInj);
			/// Update active & reactive power injection (VD bus)
			void updatePowerInjection(Complex powerInj);
			/// Get Apparent power of Powerflow solution
			Complex getApparentPower() { return Complex (mSetPointActivePower, mSetPointReactivePower);}
		};
}
}
}
