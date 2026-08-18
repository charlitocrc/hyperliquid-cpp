#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

// Read-only queries for staking, borrow/lend and prediction markets.
// No private key needed.
//
// Pass an address as argv[1] to inspect your own account; the default is a
// validator, which is the easiest way to see populated staking data.

int main(int argc, char** argv) {
    try {
        const std::string user =
            argc > 1 ? argv[1] : "0x000000000056f99d36b6f2e0c51fd41496bbacb8";

        hyperliquid::Info info(hyperliquid::MAINNET_API_URL);

        // --- Staking ---------------------------------------------------
        // All amounts are HYPE as float strings. undelegated is stake sitting
        // in the staking balance but not assigned to a validator; it earns
        // nothing until delegated.
        std::cout << "=== userStakingSummary(" << user << ") ===\n";
        auto summary = info.userStakingSummary(user);
        std::cout << "  delegated:              " << summary.value("delegated", "?") << "\n";
        std::cout << "  undelegated:            " << summary.value("undelegated", "?") << "\n";
        std::cout << "  totalPendingWithdrawal: "
                  << summary.value("totalPendingWithdrawal", "?") << "\n";
        std::cout << "  nPendingWithdrawals:    " << summary.value("nPendingWithdrawals", 0) << "\n";

        // Stake is locked for a period after delegating; lockedUntilTimestamp
        // is when it can be undelegated.
        std::cout << "\n=== userStakingDelegations ===\n";
        auto delegations = info.userStakingDelegations(user);
        if (delegations.empty()) {
            std::cout << "  Not delegated to any validator.\n";
        }
        for (const auto& delegation : delegations) {
            std::cout << "  " << delegation.value("validator", "?")
                      << "  " << delegation.value("amount", "?")
                      << "  locked until " << delegation["lockedUntilTimestamp"] << "\n";
        }

        // Newest first. "commission" entries appear only for validators;
        // ordinary delegators see only "delegation".
        auto rewards = info.userStakingRewards(user);
        std::cout << "\n=== userStakingRewards (latest 3 of " << rewards.size() << ") ===\n";
        for (size_t i = 0; i < 3 && i < rewards.size(); ++i) {
            std::cout << "  " << rewards[i]["time"]
                      << "  " << rewards[i].value("source", "?")
                      << "  " << rewards[i].value("totalAmount", "?") << "\n";
        }

        // Delegations, undelegations, deposits and withdrawals. The delta is a
        // tagged object, so read whichever key is present.
        std::cout << "\n=== delegatorHistory (latest 3) ===\n";
        auto history = info.delegatorHistory(user);
        for (size_t i = 0; i < 3 && i < history.size(); ++i) {
            const auto& delta = history[i]["delta"];
            std::cout << "  " << history[i]["time"] << "  "
                      << delta.begin().key() << " " << delta.begin().value().dump() << "\n";
        }

        // --- Borrow / lend ---------------------------------------------
        // tokenToState is [[token index, {borrow, supply}], ...]; basis is the
        // principal, value the amount including accrued interest.
        std::cout << "\n=== borrowLendUserState ===\n";
        auto borrow_state = info.borrowLendUserState(user);
        std::cout << "  health:       " << borrow_state.value("health", "?") << "\n";
        std::cout << "  healthFactor: "
                  << (borrow_state["healthFactor"].is_null()
                          ? "null (nothing borrowed)"
                          : borrow_state["healthFactor"].get<std::string>()) << "\n";
        std::cout << "  positions:    " << borrow_state["tokenToState"].size() << "\n";
        for (const auto& entry : borrow_state["tokenToState"]) {
            std::cout << "    token " << entry[0]
                      << "  supply " << entry[1]["supply"].value("value", "?")
                      << "  borrow " << entry[1]["borrow"].value("value", "?") << "\n";
        }

        // Reserve-wide rates. An ltv of "0.0" means the token cannot be used
        // as collateral. Token 0 is USDC.
        std::cout << "\n=== borrowLendReserveState(0) ===\n";
        auto reserve = info.borrowLendReserveState(0);
        std::cout << "  supplyYearlyRate: " << reserve.value("supplyYearlyRate", "?") << "\n";
        std::cout << "  borrowYearlyRate: " << reserve.value("borrowYearlyRate", "?") << "\n";
        std::cout << "  utilization:      " << reserve.value("utilization", "?") << "\n";
        std::cout << "  totalSupplied:    " << reserve.value("totalSupplied", "?") << "\n";
        std::cout << "  ltv:              " << reserve.value("ltv", "?") << "\n";

        auto all_reserves = info.allBorrowLendReserveStates();
        std::cout << "\n=== allBorrowLendReserveStates: " << all_reserves.size()
                  << " reserves ===\n";
        for (size_t i = 0; i < 3 && i < all_reserves.size(); ++i) {
            std::cout << "  token " << all_reserves[i][0]
                      << "  utilization " << all_reserves[i][1].value("utilization", "?")
                      << "  ltv " << all_reserves[i][1].value("ltv", "?") << "\n";
        }

        // --- Prediction markets ----------------------------------------
        // Outcomes are grouped into questions; the description encodes the
        // terms, e.g. "class:priceBinary|underlying:BTC|expiry:...|targetPrice:...".
        std::cout << "\n=== outcomeMeta ===\n";
        auto outcomes_meta = info.outcomeMeta();
        const auto& outcomes = outcomes_meta["outcomes"];
        std::cout << "  " << outcomes.size() << " open outcomes, "
                  << outcomes_meta["questions"].size() << " questions, feeScale "
                  << outcomes_meta.value("feeScale", "?") << "\n";
        for (size_t i = 0; i < 3 && i < outcomes.size(); ++i) {
            std::cout << "    " << outcomes[i]["outcome"] << "  "
                      << outcomes[i].value("description", "") << "\n";
        }

        // settledOutcome returns null while an outcome is still open, so the
        // ones listed above answer null until they settle.
        std::cout << "\n=== settledOutcome ===\n";
        for (int64_t id : {int64_t{1},
                           outcomes.empty() ? int64_t{1}
                                            : outcomes[0]["outcome"].get<int64_t>()}) {
            auto settled = info.settledOutcome(id);
            std::cout << "  outcome " << id << ": ";
            if (settled.is_null()) {
                std::cout << "not settled yet\n";
            } else {
                std::cout << "settleFraction " << settled.value("settleFraction", "?")
                          << " (" << settled.value("details", "") << ")\n";
            }
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
