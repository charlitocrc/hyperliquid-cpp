#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <chrono>

int main() {
    const std::string address = "0x0000000000000000000000000000000000000000";
    const std::string coin = "ETH";
    const std::string interval = "1h";

    // Last 48 hours
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t start_time = now_ms - (48LL * 60 * 60 * 1000);

    hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

    // --- candlesSnapshot ---
    std::cout << "=== " << coin << " Candles (" << interval << ", last 48h) ===\n\n";

    auto candles = info.candlesSnapshot(coin, interval, start_time);

    if (!candles.is_array() || candles.empty()) {
        std::cout << "No candle data returned.\n";
    } else {
        std::cout << "Total candles: " << candles.size() << "\n\n";
        std::cout << "Most recent 5 candles:\n";
        int show = std::min((int)candles.size(), 5);
        for (int i = (int)candles.size() - show; i < (int)candles.size(); ++i) {
            const auto& c = candles[i];
            std::cout << "  t=" << c["t"]
                      << "  o=" << c["o"].get<std::string>()
                      << "  h=" << c["h"].get<std::string>()
                      << "  l=" << c["l"].get<std::string>()
                      << "  c=" << c["c"].get<std::string>()
                      << "  v=" << c["v"].get<std::string>()
                      << "  n=" << c["n"]
                      << "\n";
        }
    }

    // --- userTwapSliceFills ---
    std::cout << "\n=== TWAP Slice Fills for " << address << " ===\n\n";

    auto fills = info.userTwapSliceFills(address);

    if (!fills.is_array() || fills.empty()) {
        std::cout << "No TWAP slice fills found.\n";
    } else {
        std::cout << "Total records (max 2000): " << fills.size() << "\n\n";
        std::cout << "Most recent 5 entries:\n";
        int show = std::min((int)fills.size(), 5);
        for (int i = (int)fills.size() - show; i < (int)fills.size(); ++i) {
            const auto& rec = fills[i];
            const auto& f = rec["fill"];
            std::cout << "  twapId=" << rec["twapId"]
                      << "  coin=" << f["coin"].get<std::string>()
                      << "  side=" << f["side"].get<std::string>()
                      << "  sz=" << f["sz"].get<std::string>()
                      << "  px=" << f["px"].get<std::string>()
                      << "  dir=" << f["dir"].get<std::string>()
                      << "\n";
        }
    }

    return 0;
}
