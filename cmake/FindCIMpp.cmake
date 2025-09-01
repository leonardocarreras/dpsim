cmake_minimum_required(VERSION 3.12)

# Inputs
set(CIM_VERSION "${CIM_VERSION}" CACHE STRING "CIM version (e.g. CGMES_2.4.15_16FEB2016)")
set(CIMPP_ROOT  "${CIMPP_ROOT}"  CACHE PATH   "CIMpp install prefix (/usr, /usr/local, /opt/cimpp)")
if(NOT CIM_VERSION OR CIM_VERSION STREQUAL "")
	set(CIM_VERSION "CGMES_2.4.15_16FEB2016")
endif()

# Detect CGMES builds and expose a flag
set(CGMES_BUILD OFF)
if(CIM_VERSION MATCHES "^CGMES_")
	set(CGMES_BUILD ON)
endif()

# Hints
set(_hints ${CIMPP_ROOT} $ENV{CIMPP_ROOT} /usr /usr/local /opt/cimpp)

# ---- Required header roots (use your exact sentinel files)
# src: .../include/cimpp/src
find_path(CIMPP_SRC_INCLUDE_DIR
	NAMES CIMFile.hpp
	HINTS ${_hints}
	PATH_SUFFIXES include/cimpp/src include/cimpp/${CIM_VERSION}/src)

# version: .../include/cimpp/<version>
find_path(CIMPP_VER_INCLUDE_DIR
	NAMES Line.hpp
	HINTS ${_hints}
	PATH_SUFFIXES include/cimpp/${CIM_VERSION})

# static: .../include/cimpp/static
find_path(CIMPP_STATIC_INCLUDE_DIR
	NAMES BaseClass.hpp
	HINTS ${_hints}
	PATH_SUFFIXES include/cimpp/static)

# static/IEC61970: .../include/cimpp/static/IEC61970
find_path(CIMPP_STATIC_IEC61970_INCLUDE_DIR
	NAMES IEC61970CIMVersion.h
	HINTS ${_hints}
	PATH_SUFFIXES include/cimpp/static/IEC61970)

# ---- Library (include explicit CGMES name then fallbacks)
find_library(CIMPP_LIBRARY
	NAMES cimpp${CIM_VERSION} cimppCGMES_2.4.15_16FEB2016 cimpp
	HINTS ${_hints}
	PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu)

# ---- Validate
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CIMpp
	REQUIRED_VARS
	CIMPP_LIBRARY
	CIMPP_SRC_INCLUDE_DIR
	CIMPP_VER_INCLUDE_DIR
	CIMPP_STATIC_INCLUDE_DIR
	CIMPP_STATIC_IEC61970_INCLUDE_DIR
	CIMPP_ROOT_INCLUDE_DIR
	FAIL_MESSAGE "Set CIMPP_ROOT and CIM_VERSION so that src, <version>, static and static/IEC61970 are found.")

# ---- Target
if(NOT TARGET CIMPP::cimpp)
	add_library(CIMPP::cimpp UNKNOWN IMPORTED)
	# expose all four include roots
	set(_incs
		"${CIMPP_SRC_INCLUDE_DIR}"
		"${CIMPP_VER_INCLUDE_DIR}"
		"${CIMPP_STATIC_INCLUDE_DIR}"
		"${CIMPP_STATIC_IEC61970_INCLUDE_DIR}")
	list(REMOVE_DUPLICATES _incs)
	set_target_properties(CIMPP::cimpp PROPERTIES
	IMPORTED_LOCATION "${CIMPP_LIBRARY}"
	INTERFACE_INCLUDE_DIRECTORIES "${_incs}")
endif()

# (optional) diagnostics
message(STATUS "[CIMpp] src     = ${CIMPP_SRC_INCLUDE_DIR}")
message(STATUS "[CIMpp] version  = ${CIMPP_VER_INCLUDE_DIR}")
message(STATUS "[CIMpp] static   = ${CIMPP_STATIC_INCLUDE_DIR}")
message(STATUS "[CIMpp] IEC61970 = ${CIMPP_STATIC_IEC61970_INCLUDE_DIR}")
message(STATUS "[CIMpp] lib      = ${CIMPP_LIBRARY}")
