set(TESTS
    trailing_stop_test
    claim_rewards_test
    info_meta_test
    info_perp_market_queries_test
    info_perp_deployment_queries_test
    info_spot_queries_test
    info_outcome_queries_test
    info_staking_queries_test
    info_borrow_lend_queries_test
    info_account_queries_test
    twap_wire_test
    twap_order_test
    twap_cancel_test
    top_up_isolated_only_margin_test
    reserve_request_weight_test
    usd_class_transfer_test
    send_asset_test
    approve_builder_fee_test
    approve_agent_test
    l1_action_signing_test
    noop_signing_test
    set_referrer_signing_test
    create_sub_account_signing_test
    sub_account_transfer_signing_test
    sub_account_spot_transfer_signing_test
    cancel_signing_test
    cancel_by_cloid_signing_test
    update_leverage_signing_test
    schedule_cancel_signing_test
    twap_order_signing_test
    twap_cancel_signing_test
    add_multi_sig_types_test
    add_multi_sig_fields_test
    multi_sig_l1_payload_test
    multi_sig_user_signed_payload_test
    multi_sig_action_test
    convert_to_multi_sig_user_signers_test
    convert_to_multi_sig_user_test
    multi_sig_test
    spot_token_index_test
    c_deposit_test
    c_withdraw_test
    token_delegate_test
    vault_transfer_test
    withdraw_from_bridge_test
    outcome_signing_test
    outcome_encoding_test
    outcome_order_test
    outcome_market_order_test
    builder_perp_order_test
    split_outcome_test
    merge_outcome_test
    merge_question_test
    negate_outcome_test
    activate_outcome_deployer_test
    deactivate_outcome_deployer_test
    register_standalone_outcome_from_template_test
    register_question_from_template_test
    register_and_associate_named_outcome_from_template_test
    settle_outcome_test
    settle_question_test
    set_outcome_sub_deployers_test
    spot_deploy_test
    perp_deploy_test
)

if(BUILD_WEBSOCKET)
    list(APPEND TESTS websocket_identifier_test ws_types_test ws_post_test)
endif()

foreach(TEST ${TESTS})
    add_executable(${TEST} ${TEST}.cpp)
    target_link_libraries(${TEST} PRIVATE hyperliquid)

    # These tests assert with assert(). A Release build defines NDEBUG, which
    # compiles every assert to nothing and makes the suite pass vacuously.
    # Force asserts on regardless of build type.
    target_compile_options(${TEST} PRIVATE -UNDEBUG)

    add_test(NAME ${TEST} COMMAND ${TEST})
endforeach()
