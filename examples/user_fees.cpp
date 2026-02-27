#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>

int main() {
    const std::string address = "0x0000000000000000000000000000000000000000";
    // A commonly-used builder address to test maxBuilderFee against
    const std::string builder = "0x476fa87b4d3818f437f38f1263bee508d7672d82";

    hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

    // --- userFees ---
    std::cout << "=== User Fees for " << address << " ===\n\n";

    auto fees = info.userFees(address);

    std::cout << "Effective rates:\n"
              << "  Perp cross (taker): " << fees["userCrossRate"].get<std::string>() << "\n"
              << "  Perp add  (maker):  " << fees["userAddRate"].get<std::string>() << "\n"
              << "  Spot cross (taker): " << fees["userSpotCrossRate"].get<std::string>() << "\n"
              << "  Spot add  (maker):  " << fees["userSpotAddRate"].get<std::string>() << "\n"
              << "  Active referral discount: " << fees["activeReferralDiscount"].get<std::string>() << "\n";

    if (!fees["activeStakingDiscount"].is_null()) {
        auto& disc = fees["activeStakingDiscount"];
        std::cout << "  Active staking discount: " << disc["discount"].get<std::string>()
                  << " (at " << disc["bpsOfMaxSupply"].get<std::string>() << " bps of max supply)\n";
    }

    auto& schedule = fees["feeSchedule"];
    std::cout << "\nBase fee schedule:\n"
              << "  Perp cross: " << schedule["cross"].get<std::string>() << "\n"
              << "  Perp add:   " << schedule["add"].get<std::string>() << "\n"
              << "  Spot cross: " << schedule["spotCross"].get<std::string>() << "\n"
              << "  Spot add:   " << schedule["spotAdd"].get<std::string>() << "\n";

    if (fees["dailyUserVlm"].is_array() && !fees["dailyUserVlm"].empty()) {
        auto& latest = fees["dailyUserVlm"].back();
        std::cout << "\nMost recent daily volume (" << latest["date"].get<std::string>() << "):\n"
                  << "  User cross: " << latest["userCross"].get<std::string>() << "\n"
                  << "  User add:   " << latest["userAdd"].get<std::string>() << "\n";
    }

    // --- maxBuilderFee ---
    std::cout << "\n=== Max Builder Fee ===\n\n"
              << "User:    " << address << "\n"
              << "Builder: " << builder << "\n";

    auto fee = info.maxBuilderFee(address, builder);
    std::cout << "Approved max fee: " << fee
              << " tenths of a bps";
    if (fee.is_number()) {
        double bps = fee.get<int>() / 10000.0 * 100.0;
        std::cout << " (" << bps << "%)";
    }
    std::cout << "\n";

    return 0;
}
