#!/bin/bash

# SPDX-FileCopyrightText: 2022-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

npm install -D --save autoprefixer
npm install -D --save postcss-cli

hugo --minify
hugo server --bind 0.0.0.0 -D
