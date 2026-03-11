---
title: "RT Fit-and-Assign"
linkTitle: "RT Fit-and-Assign"
weight: 4
date: 2026-03-11
---

This page documents a proposed real-time scheduling extension for DPsim with three goals:

1. estimate whether a model is likely to fit into a real-time time step,
2. assign simulation tasks to a fixed set of worker threads and CPU cores,
3. make the runtime behavior reproducible enough to reason about deadline misses.

The motivation is that real-time overruns in DPsim are often determined by the full step critical path rather than by solver throughput alone.
In particular, interface tasks are part of the simulation task graph and can dominate the step time in tightly synchronized co-simulation setups.

# Current Behavior

## Simulation scheduling pipeline

DPsim prepares and schedules the task graph before the simulation starts.
The high-level flow is:

- `Simulation::sync()` exchanges initial interface data,
- `Simulation::prepSchedule()` collects solver, interface, and logger tasks,
- `Simulation::schedule()` resolves dependencies and builds the schedule.

The relevant implementation points are:

- `Simulation::sync()` in [dpsim/src/Simulation.cpp](../../../../../dpsim/src/Simulation.cpp#L174-L181)
- `Simulation::prepSchedule()` in [dpsim/src/Simulation.cpp](../../../../../dpsim/src/Simulation.cpp#L185-L206)
- `Simulation::schedule()` in [dpsim/src/Simulation.cpp](../../../../../dpsim/src/Simulation.cpp#L208-L211)

If no scheduler is configured explicitly, DPsim falls back to the sequential scheduler.
This means that simply pinning a process to many CPUs does not automatically parallelize the simulation.

## What an overrun means today

DPsim currently measures step time around the whole `step()` execution.
That includes:

- event handling,
- solver task execution,
- interface `PreStep` and `PostStep` tasks,
- logger tasks.

The measurement and the current overrun check are implemented in:

- `Simulation::step()` in [dpsim/src/Simulation.cpp](../../../../../dpsim/src/Simulation.cpp#L370-L388)
- `Simulation::checkForOverruns()` in [dpsim/src/Simulation.cpp](../../../../../dpsim/src/Simulation.cpp#L411-L423)

As a consequence, an overrun does **not** necessarily mean that the numerical solver alone exceeded the budget.
It only means that the full per-step critical path exceeded the configured time step.

## Why interface timing matters

Interfaces are added to the same task graph as solver tasks in `Simulation::prepSchedule()`.
For the VILLAS queueless interface this means that the per-step read and write operations are part of the measured step time.

Relevant code paths:

- interface task insertion in [dpsim/src/Simulation.cpp](../../../../../dpsim/src/Simulation.cpp#L193-L198)
- queueless interface tasks in [dpsim-villas/src/InterfaceVillasQueueless.cpp](../../../../../dpsim-villas/src/InterfaceVillasQueueless.cpp#L189-L198)
- `PreStep::execute()` in [dpsim-villas/src/InterfaceVillasQueueless.cpp](../../../../../dpsim-villas/src/InterfaceVillasQueueless.cpp#L201-L212)
- `readFromVillas()` in [dpsim-villas/src/InterfaceVillasQueueless.cpp](../../../../../dpsim-villas/src/InterfaceVillasQueueless.cpp#L214-L281)
- `PostStep::execute()` and `writeToVillas()` in [dpsim-villas/src/InterfaceVillasQueueless.cpp](../../../../../dpsim-villas/src/InterfaceVillasQueueless.cpp#L283-L349)

This is important for tightly synchronized co-simulation, because the interface can become the dominant part of the step critical path.

## Blocking and synchronization semantics

The generic interface API stores two import flags:

- `blockOnRead`
- `syncOnSimulationStart`

The generic storage is implemented in [dpsim/include/dpsim/Interface.h](../../../../../dpsim/include/dpsim/Interface.h#L42-L54) and [dpsim/src/Interface.cpp](../../../../../dpsim/src/Interface.cpp#L7-L18).

For queued interfaces, these flags influence whether the simulation blocks on imported values and whether startup synchronization waits for a first remote value.
However, for `InterfaceVillasQueueless`, the current per-step path always executes `readFromVillas()` in `PreStep`.
In that implementation, `syncOnSimulationStart` is used by `syncImports()`, but `blockOnRead` is not currently used to change the per-step execution strategy.

This means that for queueless co-simulation, the main distinction today is mostly:

- startup synchronization behavior, and
- the direct cost of per-step read and write operations.

A concrete example is the 9-bus 4th-order co-simulation example, where the import flags depend on the chosen transport in [dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim.cpp](../../../../../dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim.cpp#L767-L776).

# Problem Statement

Real-time execution on a `PREEMPT_RT` kernel can still produce more overruns than a non-RT kernel if the model is close to the deadline and the measured critical path includes:

- interface communication,
- synchronization overhead,
- barrier costs,
- thread wakeup and scheduling overhead,
- logger work.

This is especially visible for small time steps, where a few microseconds of additional cost are enough to turn a borderline case into a consistently overrunning one.

In other words, a real-time kernel improves determinism, but it does not automatically make an overloaded task graph fit into the deadline.

# Proposed RT Fit-and-Assign Design

The proposed extension adds an RTDS-like startup flow with explicit admission testing and deterministic worker placement.

## Goals

- predict whether a simulation configuration is likely to fit into the configured time step,
- compute a stable task-to-worker assignment before entering the real-time loop,
- pin workers to fixed CPUs,
- make interface-heavy models diagnosable.

## Non-goals

Phase 1 does **not** attempt to solve all scheduling problems.
In particular, it does not try to:

- make OpenMP the primary RT execution path,
- perform dynamic load balancing during the run,
- hide interface latency that is fundamentally on the critical path.

# Phase 1 Scope

Phase 1 targets `ThreadLevelScheduler` and its worker thread infrastructure.
This path is preferred because it already has explicit worker management and task assignment points.

Relevant files:

- [dpsim/src/ThreadLevelScheduler.cpp](../../../../../dpsim/src/ThreadLevelScheduler.cpp#L26-L132)
- [dpsim/src/ThreadScheduler.cpp](../../../../../dpsim/src/ThreadScheduler.cpp#L35-L122)

OpenMP remains useful for experimentation, but it is not the first target for deterministic RT placement.

# Startup Admission Test

Before starting the real-time loop, the scheduler should estimate whether the simulation fits into the time step.

## Inputs

The admission test should use:

- the resolved task graph,
- the levelized schedule,
- measured or previously recorded task execution times,
- the configured time step,
- the available worker-core set.

Measurement infrastructure already exists in the scheduler base class:

- measurement collection in [dpsim/src/Scheduler.cpp](../../../../../dpsim/src/Scheduler.cpp#L21-L32)
- measurement persistence in [dpsim/src/Scheduler.cpp](../../../../../dpsim/src/Scheduler.cpp#L32-L63)
- averaged measurement lookup in [dpsim/src/Scheduler.cpp](../../../../../dpsim/src/Scheduler.cpp#L65-L73)

## Estimation model

The initial admission test should compute two bounds:

1. a **critical-path bound** based on task dependencies,
2. a **per-core load bound** based on assigning tasks to a finite worker set.

A run should be reported as:

- **admitted** if both bounds are clearly below the time step,
- **warning** if the margin is small,
- **rejected** if the predicted bound exceeds the time step.

A conservative rule is better than an optimistic one.
For real-time use, false negatives are preferable to false positives.

# Task-to-Core Assignment

After admission, the scheduler should compute a static mapping from tasks to worker threads.
Each worker thread is then pinned to one CPU.

## Assignment strategy

The first implementation should use a level-based min-max heuristic:

- preserve dependency order,
- assign tasks level by level,
- minimize the maximum estimated worker load per level,
- keep interface tasks explicitly in the model.

This is a good match for the current level scheduler structure in [dpsim/src/ThreadLevelScheduler.cpp](../../../../../dpsim/src/ThreadLevelScheduler.cpp#L81-L132).

## Why static assignment

Static assignment is preferred for short, repetitive real-time steps because it reduces:

- task migration,
- cache instability,
- per-step scheduling decisions,
- timing jitter.

# Worker Realization and CPU Pinning

Once the assignment is known, worker threads should be created with a fixed CPU affinity.

The natural insertion point is the thread scheduler worker creation code in [dpsim/src/ThreadScheduler.cpp](../../../../../dpsim/src/ThreadScheduler.cpp#L52-L63) and the worker loop in [dpsim/src/ThreadScheduler.cpp](../../../../../dpsim/src/ThreadScheduler.cpp#L94-L122).

The phase-1 implementation should add:

- a worker-to-CPU mapping,
- affinity setup at worker creation time,
- runtime validation and logging of the chosen assignment.

# Interface-Aware Scheduling

The fit-and-assign algorithm must treat interface tasks as first-class tasks.
This is required because in real-time co-simulation the interface is often part of the critical path, not just background I/O.

The design should therefore:

- include `PreStep` and `PostStep` timing in the admission test,
- allow interface tasks to be assigned intentionally rather than incidentally,
- optionally reserve a worker or CPU for communication-heavy tasks when beneficial.

# Why Multi-Core Can Be Slower Than Single-Core

Using more cores is only useful if the task graph contains enough independent work outside the critical path.
For small time steps, multi-core execution can be slower because of:

- barrier overhead,
- worker wakeup cost,
- cache effects,
- load imbalance between levels,
- interface latency that cannot be parallelized away.

This explains why a single core can outperform a multi-core configuration for some tightly coupled EMT and co-simulation workloads.
The admission test should make this visible before the real-time run starts.

# Validation Plan

The design should be validated against:

- predicted critical-path budget vs measured step times,
- timer overruns vs step-time overruns,
- single-core vs multi-core runs,
- models with and without queueless interfaces.

A suitable benchmark is the 9-bus 4th-order co-simulation example in [dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim.cpp](../../../../../dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim.cpp).

# Future Work

Potential phase-2 items include:

- an explicit interface mode that separates startup synchronization from per-step blocking semantics for queueless transports,
- OpenMP-aware admission testing,
- automatic reuse of previous timing profiles,
- per-task worst-case rather than average-time budgeting,
- user-facing reports that explain why a model does or does not fit.
