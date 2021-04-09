set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR ${PROJECT_AUTHOR})
set(CPACK_PACKAGE_CONTACT "Steffen Vogel <stvogel@eonerc.rwth-aachen.de")
set(CPACK_PACKAGE_VERSION ${DPSIM_SHORT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${DPSIM_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${DPSIM_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${DPSIM_PATH_VERSION})
set(CPACK_RPM_PACKAGE_RELEASE ${DPSIM_RELEASE})

set(CPACK_RPM_PACKAGE_VERSION ${DPSIM_VERSION})
set(CPACK_RPM_PACKAGE_RELEASE ${DPSIM_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_LICENSE "MPL 2.0")
set(CPACK_RPM_PACKAGE_REQUIRES "libcimpp16v29a libvillas graphviz python3-libs python3 python3-pip")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")

# As close as possible to Fedoras naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.md")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR "RPM;TGZ")
endif()

include(CPack)
