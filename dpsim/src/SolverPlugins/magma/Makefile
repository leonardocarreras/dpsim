# SPDX-FileCopyrightText: 2022-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

CC = gcc -std=gnu99

CC_FLAGS += -g
LD_FLAGS += -g -lmagma -lmagma_sparse

.PHONY: all

all: libdps_magma.so

%.o : %.c
	$(CC) $(CC_FLAGS) -I../../../include -c -fpic -o $@ $<

libdps_magma.so: magma.o
	$(CC) -shared -o $@ $< $(LD_FLAGS)
