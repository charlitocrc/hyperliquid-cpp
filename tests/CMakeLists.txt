set(TESTS
    info_perp_deployment_queries_test
    twap_orders_test
    rate_limits_margin_test
    usd_class_transfer_test
    send_asset_test
    l1_action_signing_test
)

foreach(TEST ${TESTS})
    add_executable(${TEST} ${TEST}.cpp)
    target_link_libraries(${TEST} PRIVATE hyperliquid)

    # These tests assert with assert(). A Release build defines NDEBUG, which
    # compiles every assert to nothing and makes the suite pass vacuously.
    # Force asserts on regardless of build type.
    target_compile_options(${TEST} PRIVATE -UNDEBUG)

    add_test(NAME ${TEST} COMMAND ${TEST})
endforeach()
