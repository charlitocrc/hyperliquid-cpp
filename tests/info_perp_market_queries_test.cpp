#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void predictedFundingsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.predictedFundings();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "predictedFundings"}});
}

void perpsAtOpenInterestCapOmitsEmptyDex() {
    TestInfo info;

    const auto response = info.perpsAtOpenInterestCap();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpsAtOpenInterestCap"}});
}

void perpsAtOpenInterestCapIncludesDexWhenProvided() {
    TestInfo info;

    const auto response = info.perpsAtOpenInterestCap("xyz");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpsAtOpenInterestCap"}, {"dex", "xyz"}});
}

void perpCategoriesUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpCategories();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpCategories"}});
}

void perpConciseAnnotationsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpConciseAnnotations();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpConciseAnnotations"}});
}

void perpAnnotationUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpAnnotation("xyz:TSLA");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpAnnotation"}, {"coin", "xyz:TSLA"}});
}

void perpAnnotationRejectsEmptyCoin() {
    TestInfo info;

    bool threw = false;
    try {
        info.perpAnnotation("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
    assert(info.requests().empty());
}

void metaAndAssetCtxsUsesDefaultDexWhenUnset() {
    TestInfo info;

    const auto response = info.metaAndAssetCtxs();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "metaAndAssetCtxs"}});
}

void metaAndAssetCtxsIncludesDexWhenProvided() {
    TestInfo info;

    const auto response = info.metaAndAssetCtxs("test-dex");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "metaAndAssetCtxs"}, {"dex", "test-dex"}});
}

void checkAllPerpMetas() {
    {
        TestInfo info;
        info.allPerpMetas();
        assertSingleRequest(info, {{"type", "allPerpMetas"}});
    }
}

} // namespace

int main() {
    predictedFundingsUsesExpectedPayload();
    perpsAtOpenInterestCapOmitsEmptyDex();
    perpsAtOpenInterestCapIncludesDexWhenProvided();
    perpCategoriesUsesExpectedPayload();
    perpConciseAnnotationsUsesExpectedPayload();
    perpAnnotationUsesExpectedPayload();
    perpAnnotationRejectsEmptyCoin();
    metaAndAssetCtxsUsesDefaultDexWhenUnset();
    metaAndAssetCtxsIncludesDexWhenProvided();
    checkAllPerpMetas();
    return 0;
}
