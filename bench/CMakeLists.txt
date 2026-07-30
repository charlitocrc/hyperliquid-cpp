# Benchmarks. Off by default; enable with -DBUILD_BENCH=ON.
#
# Links OpenSSL::Crypto directly: the hyperliquid target keeps it PRIVATE, and
# the benchmark reaches past the public API to time individual crypto stages.

add_executable(signing_bench signing_bench.cpp)
target_link_libraries(signing_bench PRIVATE hyperliquid OpenSSL::Crypto)

# Release is what these numbers are only meaningful in -- a Debug build reports
# OpenSSL at its normal speed but the SDK's own stages several times slower,
# which silently misattributes where the time goes.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(WARNING "signing_bench: CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}', "
                    "not Release. Timings will not be representative.")
endif()
