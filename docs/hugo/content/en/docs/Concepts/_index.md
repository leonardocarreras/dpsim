---
title: "Concepts"
linkTitle: "Concepts"
weight: 4
menu:
  main:
    weight: 24
description: >
  The mathematics behind the solvers and the component models.
aliases: ["/docs/concepts/"]
---

<!--
SPDX-FileCopyrightText: 2022-2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
SPDX-License-Identifier: MPL-2.0
-->

The methods DPsim implements, described independently of the code. Nodal analysis and dynamic
phasors underpin the main solver and have a page each. The [model]({{< ref "Models" >}}) pages
give the physical equations for a component and how they are transformed for each supported
domain.

DPsim also includes a load flow solver, used on its own or to compute the initial state of a
network when that state is not part of the network data. Electromagnetic transient models exist
alongside the dynamic phasor ones, serving both as a simulation domain and as the reference the
dynamic phasor models are tested against.
