#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>

int main() {
    const std::string address = "0x0000000000000000000000000000000000000000";
    const std::string vault = "0x010461c14e146ac35fe42271bdc1134ee31c703a";

    hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

    // --- querySubAccounts ---
    std::cout << "=== Sub-Accounts for " << address << " ===\n";
    auto sub_accounts = info.querySubAccounts(address);
    if (sub_accounts.is_null()) {
        std::cout << "  No sub-accounts found.\n";
    } else if (sub_accounts.is_array()) {
        std::cout << "  Found " << sub_accounts.size() << " sub-account(s)\n";
        for (const auto& sa : sub_accounts) {
            std::cout << "  - " << sa["name"].get<std::string>()
                      << " (" << sa["subAccountUser"].get<std::string>() << ")\n";
        }
    } else {
        std::cout << "  Response: " << sub_accounts.dump(2) << "\n";
    }

    // --- queryReferralState ---
    std::cout << "\n=== Referral State for " << address << " ===\n";
    auto referral = info.queryReferralState(address);
    std::cout << "  Cumulative VLM: " << referral["cumVlm"].get<std::string>() << "\n"
              << "  Unclaimed rewards: " << referral["unclaimedRewards"].get<std::string>() << "\n"
              << "  Claimed rewards: " << referral["claimedRewards"].get<std::string>() << "\n";
    if (!referral["referredBy"].is_null()) {
        std::cout << "  Referred by: " << referral["referredBy"]["referrer"].get<std::string>()
                  << " (code: " << referral["referredBy"]["code"].get<std::string>() << ")\n";
    } else {
        std::cout << "  Not referred by anyone.\n";
    }
    if (!referral["referrerState"].is_null()) {
        auto& rs = referral["referrerState"];
        std::cout << "  Referrer stage: " << rs["stage"].get<std::string>() << "\n";
        if (rs.contains("data") && rs["data"].contains("referralStates")) {
            std::cout << "  Referred users: " << rs["data"]["referralStates"].size() << "\n";
        }
    }

    // --- approvedBuilders ---
    std::cout << "\n=== Approved Builders for " << address << " ===\n";
    auto builders = info.approvedBuilders(address);
    if (builders.is_array()) {
        if (builders.empty()) {
            std::cout << "  No approved builders.\n";
        } else {
            for (const auto& b : builders) {
                std::cout << "  - " << b.get<std::string>() << "\n";
            }
        }
    } else {
        std::cout << "  Response: " << builders.dump(2) << "\n";
    }

    // --- userRole ---
    std::cout << "\n=== User Role for " << address << " ===\n";
    auto role = info.userRole(address);
    std::cout << "  Role: " << role["role"].get<std::string>() << "\n";
    if (role.contains("data")) {
        std::cout << "  Data: " << role["data"].dump() << "\n";
    }

    // Also check vault role
    std::cout << "\n=== User Role for vault " << vault << " ===\n";
    auto vault_role = info.userRole(vault);
    std::cout << "  Role: " << vault_role["role"].get<std::string>() << "\n";

    // --- userRateLimit ---
    std::cout << "\n=== Rate Limit for " << address << " ===\n";
    auto rate_limit = info.userRateLimit(address);
    std::cout << "  Cumulative VLM: " << rate_limit["cumVlm"].get<std::string>() << "\n"
              << "  Requests used: " << rate_limit["nRequestsUsed"].get<int>() << "\n"
              << "  Requests cap: " << rate_limit["nRequestsCap"].get<int>() << "\n"
              << "  Requests surplus: " << rate_limit["nRequestsSurplus"].get<int>() << "\n";

    // --- portfolio ---
    std::cout << "\n=== Portfolio for " << address << " ===\n";
    auto portfolio = info.portfolio(address);
    if (portfolio.is_array()) {
        for (const auto& period : portfolio) {
            if (period.is_array() && period.size() == 2) {
                std::string period_name = period[0].get<std::string>();
                auto& data = period[1];
                std::cout << "  Period: " << period_name
                          << " | VLM: " << data["vlm"].get<std::string>();
                if (data.contains("accountValueHistory") && data["accountValueHistory"].is_array()) {
                    std::cout << " | Data points: " << data["accountValueHistory"].size();
                }
                std::cout << "\n";
            }
        }
    } else {
        std::cout << "  Response: " << portfolio.dump(2) << "\n";
    }

    // --- userNonFundingLedgerUpdates ---
    std::cout << "\n=== Non-Funding Ledger Updates for " << address << " ===\n";
    // Query last 7 days
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t seven_days_ago = now_ms - (7LL * 24 * 60 * 60 * 1000);
    auto ledger = info.userNonFundingLedgerUpdates(address, seven_days_ago);
    if (ledger.is_array()) {
        std::cout << "  Found " << ledger.size() << " ledger update(s) in last 7 days\n";
        for (size_t i = 0; i < std::min(ledger.size(), (size_t)3); ++i) {
            auto& entry = ledger[i];
            if (entry.contains("delta") && entry["delta"].contains("type")) {
                std::cout << "  - Type: " << entry["delta"]["type"].get<std::string>();
                if (entry["delta"].contains("usdc")) {
                    std::cout << " | USDC: " << entry["delta"]["usdc"].get<std::string>();
                }
                std::cout << "\n";
            }
        }
        if (ledger.size() > 3) {
            std::cout << "  ... and " << (ledger.size() - 3) << " more\n";
        }
    } else {
        std::cout << "  Response: " << ledger.dump(2) << "\n";
    }

    // --- vaultDetails ---
    std::cout << "\n=== Vault Details for " << vault << " ===\n";
    auto vault_details = info.vaultDetails(vault);
    if (vault_details.is_null()) {
        std::cout << "  Vault not found.\n";
    } else {
        if (vault_details.contains("name")) {
            std::cout << "  Name: " << vault_details["name"].get<std::string>() << "\n";
        }
        if (vault_details.contains("leader")) {
            std::cout << "  Leader: " << vault_details["leader"].get<std::string>() << "\n";
        }
        if (vault_details.contains("apr")) {
            std::cout << "  APR: " << vault_details["apr"].get<double>() << "\n";
        }
        if (vault_details.contains("isClosed")) {
            std::cout << "  Is closed: " << (vault_details["isClosed"].get<bool>() ? "yes" : "no") << "\n";
        }
        if (vault_details.contains("followers") && vault_details["followers"].is_array()) {
            std::cout << "  Followers: " << vault_details["followers"].size() << "\n";
        }
        if (vault_details.contains("leaderCommission")) {
            std::cout << "  Leader commission: " << vault_details["leaderCommission"].get<double>() << "\n";
        }
        if (vault_details.contains("allowDeposits")) {
            std::cout << "  Allow deposits: " << (vault_details["allowDeposits"].get<bool>() ? "yes" : "no") << "\n";
        }
    }

    // --- queryUserDexAbstractionState ---
    std::cout << "\n=== DEX Abstraction State for " << address << " ===\n";
    auto dex_abstraction = info.queryUserDexAbstractionState(address);
    if (dex_abstraction.is_boolean()) {
        std::cout << "  DEX abstraction enabled: " << (dex_abstraction.get<bool>() ? "yes" : "no") << "\n";
    } else {
        std::cout << "  DEX abstraction state: " << dex_abstraction.dump() << "\n";
    }

    // --- queryUserAbstractionState ---
    std::cout << "\n=== User Abstraction Mode for " << address << " ===\n";
    auto abstraction = info.queryUserAbstractionState(address);
    if (abstraction.is_string()) {
        std::cout << "  Mode: " << abstraction.get<std::string>() << "\n";
    } else {
        std::cout << "  Response: " << abstraction.dump() << "\n";
    }

    // --- userVaultEquities ---
    std::cout << "\n=== Vault Equities for " << address << " ===\n";
    auto vault_equities = info.userVaultEquities(address);
    if (vault_equities.is_array()) {
        if (vault_equities.empty()) {
            std::cout << "  No vault positions.\n";
        } else {
            for (const auto& ve : vault_equities) {
                std::cout << "  - Vault: " << ve["vaultAddress"].get<std::string>()
                          << " | Equity: " << ve["equity"].get<std::string>() << "\n";
            }
        }
    } else {
        std::cout << "  Response: " << vault_equities.dump(2) << "\n";
    }

    std::cout << "\nAll account queries completed successfully.\n";
    return 0;
}
