# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0

{
  fetchFromGitHub,
  sundials,
  python2,
}:
(sundials.overrideAttrs (
  finalAttrs: prevAttrs: {
    version = "3.2.1";
    src = fetchFromGitHub {
      owner = "LLNL";
      repo = "sundials";
      rev = "v${finalAttrs.version}";
      hash = "sha256-5fVgxFEzhzw7rAENpt2+8qGR0pe00nntSFnyArmafzU";
    };

    doCheck = false;
  }
)).override
  { python = python2; }
