---
title: "Model Implementations"
linkTitle: "Model Implementations"
weight: 20
description: >
  How each model family is arranged in code, paired with its equations under Concepts.
---

<!--
SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
SPDX-License-Identifier: MPL-2.0
-->

One page per model family, covering the class hierarchy, how the component interfaces with the
solver, its attributes and state layout, and the traps in configuring it. The equations behind each
are under [Concepts]({{< ref "/docs/Concepts/Models" >}}), which names no class; these pages name
nothing else.

Which domains implement which model is in
[model availability]({{< ref "/docs/Reference/model-availability.md" >}}), generated from the
headers.
