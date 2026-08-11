#!/bin/bash

# SPDX-FileCopyrightText: 2020-2023 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

# Jupyter Extension Matplotlib Widget
pip3 install \
    nodejs \
    ipympl
pip3 install --upgrade \
    jupyterlab

jupyter labextension install \
    @jupyter-widgets/jupyterlab-manager \
    jupyter-matplotlib

jupyter nbextension install --py --symlink ipympl
jupyter nbextension enable --py ipympl
