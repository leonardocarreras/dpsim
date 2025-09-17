set(COSIMCMD_SOURCES
src/main.cpp
)

set(COSIM_SOURCES
${CMAKE_SOURCE_DIR}/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_leader_v2_sync.cpp
${CMAKE_SOURCE_DIR}/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_follower_v2_sync.cpp
${CMAKE_SOURCE_DIR}/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_baseline.cpp
)

add_executable(cosimcmd ${COSIMCMD_SOURCES})

add_custom_target(cosimulations ALL DEPENDS ${COSIM_SOURCES})

add_dependencies(cosimulations cosimcmd dpsim dpsim-models dpsim-villas)

target_compile_features(cosimcmd PRIVATE cxx_std_17)
