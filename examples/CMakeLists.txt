# Examples CMakeLists.txt

# Create executable for each example
set(EXAMPLES
    basic_order
    basic_market_order
    query_positions
    bulk_orders
)

foreach(EXAMPLE ${EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    target_link_libraries(${EXAMPLE} PRIVATE hyperliquid)
endforeach()
