# SPDX-FileCopyrightText: 2022-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

CC = gcc

.PHONY: all

all: plugin.so

%.o : %.c
	$(CC) $(CC_FLAGS) -I../../../include -c -fpic -o $@ $<

plugin.so: example.o
	$(CC) $(LD_FLAGS) -shared -o $@ $<
