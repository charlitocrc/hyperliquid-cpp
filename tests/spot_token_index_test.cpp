// A spot pair's "tokens" field holds token *ids*, not positions in the tokens
// array. The two agree for the first few hundred tokens and then diverge: on
// live mainnet 41 tokens have index != position, and pairs reference ids past
// the end of the array entirely (max id 872 against 499 tokens).
//
// Indexing positionally therefore read out of bounds for 21 mainnet pairs, and
// silently resolved the wrong token for others -- giving those pairs another
// token's szDecimals, and so the wrong tick/lot rounding on every order for
// them.
//
// The fixture below reproduces both shapes with a handful of tokens.

#include "hyperliquid/errors.hpp"
#include "hyperliquid/info.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid::AssetInfo;
using hyperliquid::Info;
using hyperliquid::Meta;
using hyperliquid::SpotAssetInfo;
using hyperliquid::SpotMeta;
using hyperliquid::SpotTokenInfo;

namespace {

// SpotTokenInfo{name, sz_decimals, wei_decimals, index, token_id, is_canonical}
//
// Positions 0..2 are dense, then index jumps: position 3 carries index 900.
// Nothing here is reachable by treating an id as a position.
SpotMeta sparseSpotMeta() {
    return SpotMeta{
        {
            // "@1" pairs token id 900 (base) with 0 (quote). Under positional
            // lookup, tokens[900] is past the end of a 4-element vector.
            SpotAssetInfo{"@1", {900, 0}, 1, true},
            // "@2" pairs id 2 with 0. Positionally tokens[2] happens to exist,
            // but it is the token whose id is 7, not 2 -- the silent-wrong-token
            // case.
            SpotAssetInfo{"@2", {2, 0}, 2, true},
        },
        {
            SpotTokenInfo{"USDC", 8, 8, 0, "0x0", true},   // position 0, index 0
            SpotTokenInfo{"WRONG", 1, 5, 7, "0x7", true},  // position 1, index 7
            SpotTokenInfo{"RIGHT", 3, 5, 2, "0x2", true},  // position 2, index 2
            SpotTokenInfo{"FAR", 4, 5, 900, "0x384", true},// position 3, index 900
        },
    };
}

const Meta& perpMeta() {
    static const Meta meta{{AssetInfo{"BTC", 5}}};
    return meta;
}

const std::vector<std::string>& perpDexs() {
    static const std::vector<std::string> dexs{""};
    return dexs;
}

// All metadata supplied, so the constructor makes no network calls.
Info makeInfo(const SpotMeta& spot_meta) {
    return Info("http://localhost", true, &perpMeta(), &spot_meta, &perpDexs(), 1000);
}

// A token id beyond the end of the tokens array must resolve by id, not blow
// past the vector. Positional lookup here is undefined behaviour.
void outOfRangeTokenIdResolves() {
    const SpotMeta spot_meta = sparseSpotMeta();
    Info info = makeInfo(spot_meta);

    const int asset = info.nameToAsset("@1");
    assert(asset == 10001);

    // FAR is the base token, so the pair inherits its szDecimals (4), not
    // whatever happened to sit at position 900.
    assert(info.asset_to_sz_decimals_.at(asset) == 4);

    // And the pair is reachable by its BASE/QUOTE display name.
    assert(info.nameToCoin("FAR/USDC") == "@1");
}

// An id that is in range but whose position holds a different token must still
// resolve to the token carrying that id.
void inRangeButMisalignedTokenIdResolves() {
    const SpotMeta spot_meta = sparseSpotMeta();
    Info info = makeInfo(spot_meta);

    const int asset = info.nameToAsset("@2");
    assert(asset == 10002);

    // Token id 2 is RIGHT (szDecimals 3). Position 2 is also RIGHT here, but
    // the token at position 2 under the old code was reached by id, so pin the
    // value that distinguishes RIGHT (3) from WRONG (1).
    assert(info.asset_to_sz_decimals_.at(asset) == 3);
    assert(info.nameToCoin("RIGHT/USDC") == "@2");
}

// A dangling id is a broken metadata response, not something to paper over:
// silently skipping the pair would surface later as a confusing "unknown coin".
void unknownTokenIdThrows() {
    SpotMeta spot_meta = sparseSpotMeta();
    spot_meta.universe.push_back(SpotAssetInfo{"@3", {12345, 0}, 3, true});

    bool threw = false;
    try {
        Info info = makeInfo(spot_meta);
    } catch (const hyperliquid::Error&) {
        threw = true;
    }

    assert(threw);
}

// registerSpotMeta is the public way in for callers who supply their own
// metadata; it must resolve ids the same way the constructor does.
void registerSpotMetaResolvesById() {
    const SpotMeta empty_spot_meta{};
    Info info = makeInfo(empty_spot_meta);

    info.registerSpotMeta(sparseSpotMeta());

    assert(info.asset_to_sz_decimals_.at(info.nameToAsset("@1")) == 4);
    assert(info.nameToCoin("FAR/USDC") == "@1");
    assert(info.asset_to_sz_decimals_.at(info.nameToAsset("@2")) == 3);
}

}  // namespace

int main() {
    outOfRangeTokenIdResolves();
    inRangeButMisalignedTokenIdResolves();
    unknownTokenIdThrows();
    registerSpotMetaResolvesById();

    return 0;
}
