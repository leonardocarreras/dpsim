# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

{
  mkShell,
  dpsim,
  python3,
  pre-commit,
  clang-tools,
  ruby
}:
mkShell {
  inputsFrom = [ dpsim ];

  packages = [
    (python3.withPackages (ps: with ps; [ numpy ]))

    pre-commit
    ruby # Required for pre-commit
    clang-tools
  ];
}
