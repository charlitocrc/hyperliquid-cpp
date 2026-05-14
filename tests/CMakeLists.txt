add_executable(info_perp_deployment_queries_test
    info_perp_deployment_queries_test.cpp
)

target_link_libraries(info_perp_deployment_queries_test
    PRIVATE
        hyperliquid
)

add_test(NAME info_perp_deployment_queries_test
         COMMAND info_perp_deployment_queries_test)
