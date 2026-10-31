#include "multi_sig_fixture.hpp"

namespace {

void convertToMultiSigUserSignersMatchesPythonSdk() {
    auto signers = hyperliquid::convertToMultiSigUserSigners(
        {"0x0000000000000000000000000000000000000001",
         "0x0000000000000000000000000000000000000000"},
        2);

    assert(signers ==
           "{\"authorizedUsers\": [\"0x0000000000000000000000000000000000000000\", "
           "\"0x0000000000000000000000000000000000000001\"], \"threshold\": 2}");
}

void convertToMultiSigUserSignersRejectsUnusableSets() {
    auto throws = [](const std::vector<std::string>& users, int threshold) {
        try {
            hyperliquid::convertToMultiSigUserSigners(users, threshold);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    const std::string user = "0x0000000000000000000000000000000000000001";
    const std::string other = "0x0000000000000000000000000000000000000002";

    assert(throws({}, 1));                    // no signers
    assert(throws({user}, 0));                // threshold below 1
    assert(throws({user, other}, 3));         // unreachable threshold: bricked account
    assert(throws({user, user}, 2));          // duplicate cannot sign twice
    assert(throws({"0xnothex"}, 1));          // malformed address
    assert(throws(std::vector<std::string>(11, user), 1));  // over the 10-user cap
}

} // namespace

int main() {
    convertToMultiSigUserSignersMatchesPythonSdk();
    convertToMultiSigUserSignersRejectsUnusableSets();
    return 0;
}
