---
title: "Writing a Model"
linkTitle: "Writing a Model"
weight: 3
description: >
  Adding a component, interfacing it with the solver, and finding out why it is wrong.
---

<!--
SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
SPDX-License-Identifier: MPL-2.0
-->

The path from an empty file to a working component: what to declare, which hooks the solver calls
and in what order, how a component built from other components is assembled, and how to debug one
that runs but produces the wrong answer.

For the equations a model should implement, see [Concepts]({{< ref "/docs/Concepts/Models" >}}).
For worked examples of finished models, see
[model implementations]({{< ref "../Model Implementations" >}}).
