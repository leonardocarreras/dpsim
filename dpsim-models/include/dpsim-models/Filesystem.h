// SPDX-FileCopyrightText: 2017-2023 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#ifndef USE_GHC_FS
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif

#include <spdlog/fmt/ostr.h>

#if FMT_VERSION >= 90000
template <> class fmt::formatter<fs::path> : public fmt::ostream_formatter {};
#endif
