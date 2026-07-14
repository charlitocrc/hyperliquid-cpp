# Examples CMakeLists.txt

# Create executable for each example
set(EXAMPLES
    basic_order
    basic_market_order
    query_positions
    bulk_orders
    funding_history
    historical_orders
    user_fees
    candles_snapshot
    account_queries
    info_perp_deployment_queries
    twap_order
    rate_limits_margin
    usd_class_transfer
    send_asset
    approve_builder_fee
)

foreach(EXAMPLE ${EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    target_link_libraries(${EXAMPLE} PRIVATE hyperliquid)
endforeach()
