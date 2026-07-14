set(TESTS
    info_perp_deployment_queries_test
    twap_orders_test
)

foreach(TEST ${TESTS})
    add_executable(${TEST} ${TEST}.cpp)
    target_link_libraries(${TEST} PRIVATE hyperliquid)

    # These tests assert with assert(). A Release build defines NDEBUG, which
    # compiles every assert to nothing and makes the suite pass vacuously.
    # Force asserts on regardless of build type.
    if(MSVC)
        target_compile_options(${TEST} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${TEST} PRIVATE -UNDEBUG)
    endif()

    add_test(NAME ${TEST} COMMAND ${TEST})
endforeach()
