# Hyperliquid C++ SDK - TODO List

Tracks the gap between this SDK and the Hyperliquid API. Sourced from the official
docs (Info / Exchange / WebSocket endpoints) and cross-checked against the request
`type` strings actually emitted by `src/info.cpp` and `src/exchange.cpp`.

## Status Summary

**Implemented today** (verified against source, not aspiration):

- Info (34 request types): `allMids`, `clearinghouseState`, `spotClearinghouseState`, `openOrders`,
  `frontendOpenOrders`, `historicalOrders`, `orderStatus`, `l2Book`, `candleSnapshot`, `meta`,
  `metaAndAssetCtxs`, `spotMeta`, `spotMetaAndAssetCtxs`, `perpDexs`, `perpDeployAuctionStatus`,
  `userFills`, `userFillsByTime`, `userFunding`, `fundingHistory`, `userNonFundingLedgerUpdates`,
  `userFees`, `userRole`, `userRateLimit`, `userTwapSliceFills`, `userVaultEquities`, `vaultDetails`,
  `subAccounts`, `referral`, `portfolio`, `maxBuilderFee`, `approvedBuilders`, `userDexAbstraction`,
  `userAbstraction`
- Exchange (16 actions): `order`, `cancel`, `cancelByCloid`, `modify`, `batchModify`, `scheduleCancel`,
  `updateLeverage`, `updateIsolatedMargin`, `topUpIsolatedOnlyMargin`, `usdSend`, `spotSend`,
  `usdClassTransfer`, `sendAsset`, `twapOrder`, `twapCancel`, `reserveRequestWeight`
- Market orders (`marketOpen` / `marketClose`), EIP-712 signing, ECDSA secp256k1 wallet,
  automatic tick/lot rounding, `setExpiresAfter`
- 4 working examples

**Not implemented at all:** WebSocket (every subscription), TWAP orders, staking, borrow/lend,
sub-accounts, vaults, agents/referrals, multi-sig, token/perp deployment, prediction-market outcomes.

---

## Critical Priority - WebSocket Support

Nothing exists yet — no `ws://` code anywhere in `src/` or `include/`. This is the single
largest gap and blocks every real-time use case.

### Infrastructure

- [ ] `WebSocketManager` class
  - [ ] Connection management (connect, disconnect, reconnect)
  - [ ] Ping/pong keepalive
  - [ ] Subscription ID tracking and message routing to callbacks
  - [ ] Thread-safe operation, queuing of subscriptions made before connect
- [ ] `subscribe()` / `unsubscribe()` / `disconnectWebsocket()`
- [ ] `subscriptionToIdentifier()`, `wsMsgToIdentifier()`, `run()`, `sendPing()`, `stop()`,
      `onMessage()`, `onOpen()`
- [ ] **WebSocket POST requests** - the WS API can carry any info request or signed action
      (`{"method": "post", "id": n, "request": {"type": "info"|"action", "payload": {...}}}`).
      Explorer requests are not supported over WS.

### Subscriptions (24 documented types)

The docs now list 24 subscriptions. `webData2` is gone — it is `webData3` now.

- [ ] `allMids` (takes optional `dex`; spot mids only ride along with the first perp dex)
- [ ] `notification`
- [ ] `webData3` — replaces the old `webData2`
- [ ] `twapStates`
- [ ] `clearinghouseState` (per-dex)
- [ ] `openOrders` (per-dex)
- [ ] `candle`
- [ ] `l2Book` (optional `nSigFigs`, `mantissa`, `fast` — 5 levels fast / 20 slow)
- [ ] `trades`
- [ ] `orderUpdates`
- [ ] `userEvents` (note: inbound channel name is `"user"`, not `"userEvents"`)
- [ ] `userFills` (optional `aggregateByTime`)
- [ ] `userFundings`
- [ ] `userNonFundingLedgerUpdates`
- [ ] `activeAssetCtx` (returns perp *or* spot ctx shape)
- [ ] `activeAssetData` (perps only)
- [ ] `userTwapSliceFills`
- [ ] `userTwapHistory`
- [ ] `bbo`
- [ ] `spotState` (optional `isPortfolioMargin`)
- [ ] `allDexsClearinghouseState`
- [ ] `allDexsAssetCtxs`
- [ ] `outcomeMetaUpdates` (prediction markets)
- [ ] `fastAssetCtxs` — payload is base64 + **raw DEFLATE** (RFC 1951, no zlib/gzip wrapper).
      Decode: base64 → inflate raw → UTF-8 → JSON. First message is a snapshot; later messages
      carry only changed coins. Will need a raw-inflate dependency (zlib with `windowBits = -15`).

### Message Types

- [ ] `WsTrade`, `WsBook`, `WsBbo`, `WsLevel`, `AllMids`, `Candle`, `Notification`
- [ ] `WsUserEvent` variants: `WsFill`, `WsUserFunding`, `WsLiquidation`, `WsNonUserCancel`
- [ ] `WsOrder` / `WsBasicOrder`
- [ ] `WsActiveAssetCtx`, `WsActiveSpotAssetCtx`, `PerpsAssetCtx`, `SpotAssetCtx`
- [ ] `WsActiveAssetData`
- [ ] `TwapState`, `WsTwapHistory`, `WsUserTwapHistory`, `WsTwapSliceFill`, `WsUserTwapSliceFills`
- [ ] `WebData3`, `PerpDexState`, `LeadingVault`
- [ ] `ClearinghouseState`, `InnerClearinghouseState`, `MarginSummary`, `AssetPosition`
- [ ] `WsSpotState`, `SpotState`, `UserBalance`
- [ ] `WsAllDexsClearinghouseState`, `WsAllDexsAssetCtxs`
- [ ] `WsOutcomeMetaUpdates`, `OutcomeSpec`, `QuestionSpec`
- [ ] `WsFastAssetCtxs`
- [ ] `WsUserNonFundingLedgerUpdate` and its 12 delta variants (deposit, withdraw,
      internalTransfer, subAccountTransfer, liquidation, vaultCreate/Deposit/Distribution,
      vaultWithdraw, vaultLeaderCommission, spotTransfer, accountClassTransfer, spotGenesis,
      rewardsClaim)

---

## High Priority - Missing Exchange Actions

### TWAP Orders — DONE

TWAP splits a large order into 30-second suborders with max 3% slippage per slice.
Implemented from the docs directly: the Python SDK has TWAP only on the read side
(`user_twap_slice_fills`), with no `twap_order` / `twap_cancel` in its `exchange.py`,
so the C++ SDK is ahead of Python here and there was no reference implementation to mirror.

- [x] `twapOrder()` - action `twapOrder`; wire `{a, b, s, r, m, t}`, size rounded to szDecimals
- [x] `twapCancel()` - action `twapCancel`; wire `{a, t}` where `t` is the twap id
- [x] `TwapWire` type with spec-ordered serialization (key order feeds the msgpack action hash)
- [x] Tests in `tests/twap_orders_test.cpp`, example in `examples/twap_order.cpp`

Duration bounds are deliberately **not** validated client-side — the docs don't publish them
(only the server error string `"Invalid TWAP duration: N min(s)"`), so guessing a range risks
rejecting orders the API would accept. We validate only size > 0 and minutes > 0.

Callers must check `response.data.status` for an `error` key: a rejected TWAP still returns
HTTP 200 with `"status": "ok"` at the top level.

### Rate Limits & Margin — DONE

Neither exists in the Python SDK; implemented from the docs directly, like TWAP.

- [x] `reserveRequestWeight()` - action `reserveRequestWeight`; wire `{type, weight}`. Buys extra
      request weight instead of being throttled. Pairs with the existing `userRateLimit()` query.
- [x] `topUpIsolatedOnlyMargin()` - action `topUpIsolatedOnlyMargin`; wire
      `{type, asset, leverage}` with leverage as a float string via `floatToWire`. Distinct from
      `updateIsolatedMargin` (fixed USDC delta): this targets a leverage and only adds margin.
- [x] Tests in `tests/rate_limits_margin_test.cpp`, example in `examples/rate_limits_margin.cpp`

### Asset Movements

- [x] `usdClassTransfer()` - action `usdClassTransfer`; spot ↔ perp. User-signed
      (`HyperliquidTransaction:UsdClassTransfer`); no vaultAddress field — a configured
      vault/subaccount rides in the signed amount as `"<amount> subaccount:<addr>"`.
      Added EIP-712 `bool` encoding for `toPerp`; signature pinned against the Python SDK
      in `tests/usd_class_transfer_test.cpp`. Example in `examples/usd_class_transfer.cpp`.
- [x] `sendAsset()` - action `sendAsset`; between dexes ("" = default perp dex, "spot" = spot).
      User-signed (`HyperliquidTransaction:SendAsset`); no vaultAddress field — a configured
      vault/subaccount is sent as the signed `fromSubAccount` field. Signature pinned against
      the Python SDK in `tests/send_asset_test.cpp`. Example in `examples/send_asset.cpp`.
- [ ] `agentSendAsset()` - action `agentSendAsset`
- [ ] `sendToEvmWithData()` - action `sendToEvmWithData`; HyperCore → HyperEVM with calldata
- [ ] `withdraw3()` - action `withdraw3`; bridge withdrawal (tracked previously as
      `withdrawFromBridge`; `withdraw3` is the actual wire name)
- [ ] `vaultTransfer()` - action `vaultTransfer`; deposit/withdraw from a vault (tracked
      previously as `vaultUsdTransfer`)
- [ ] `hip3LiquidatorTransfer()` - action `hip3LiquidatorTransfer`; deposit/withdraw from an
      HIP-3 DEX backstop liquidator
- [ ] `claimRewards()` - action `claimRewards`

### Sub-Accounts

- [ ] `createSubAccount()`
- [ ] `subAccountTransfer()`
- [ ] `subAccountSpotTransfer()`

### Agents, Referrals, Builders

- [ ] `approveAgent()` - action `approveAgent`
- [ ] `approveBuilderFee()` - action `approveBuilderFee`
- [ ] `setReferrer()`

### Abstraction

- [ ] `userDexAbstraction()` - action `userDexAbstraction`
- [ ] `agentEnableDexAbstraction()` - action `agentEnableDexAbstraction`
- [ ] `userSetAbstraction()` - action `userSetAbstraction` (`"unifiedAccount"` / `"portfolioMargin"` / `"disabled"`)
- [ ] `agentSetAbstraction()` - action `agentSetAbstraction` (`"u"` / `"p"` / `"i"`)

### Misc

- [ ] `noop()` - action `noop`; invalidates a pending nonce
- [ ] `useBigBlocks()` - big-blocks mode
- [ ] `gossipPriorityBid()` - Python SDK action, no public doc page

---

## High Priority - Missing Info Queries

### Perps (newly surfaced by docs, never tracked here)

- [ ] `predictedFundings()` - `predictedFundings`; funding across venues (first perp dex only)
- [ ] `perpsAtOpenInterestCap()` - `perpsAtOpenInterestCap`
- [ ] `allPerpMetas()` - `allPerpMetas`; meta + ctxs for every dex in one call
- [ ] `perpDexLimits()` - `perpDexLimits`; OI caps and transfer limits for a builder dex
- [ ] `perpDexStatus()` - `perpDexStatus`; total net deposit for a dex
- [ ] `perpAnnotation()` - `perpAnnotation`; category/description for a coin
- [ ] `perpCategories()` - `perpCategories`
- [ ] `perpConciseAnnotations()` - `perpConciseAnnotations`
- [ ] `activeAssetData()` - `activeAssetData`; leverage, max trade size, available-to-trade.
      Currently only reachable via WebSocket in the Python SDK; the info endpoint supports it too.

Also note: the `meta` response now carries `marginTables` and per-asset `marginMode`
(`"strictIsolated"` / `"noCross"`) plus `growthMode`. `onlyIsolated` is deprecated.
Our `Meta` type does not model any of these.

- [ ] Extend `Meta` / `AssetInfo` with `marginTables`, `marginTableId`, `marginMode`,
      `growthMode`, `collateralToken`, `isDelisted`

### Spot

- [ ] `tokenDetails()` - `tokenDetails`; supply, genesis, deployer, deploy gas
- [ ] `spotDeployState()` - `spotDeployState`; spot deploy auction state
- [ ] `spotPairDeployAuctionStatus()` - `spotPairDeployAuctionStatus`

### Prediction Markets / Outcomes (entirely new surface)

- [ ] `outcomeMeta()` - `outcomeMeta`
- [ ] `settledOutcome()` - `settledOutcome`
- [ ] `userOutcome()` exchange action - split / merge outcome, merge question, negate outcome

### Staking

- [ ] `userStakingSummary()` - delegated, undelegated, pending withdrawals
- [ ] `userStakingDelegations()` - per-validator, with locked-until timestamps
- [ ] `userStakingRewards()` - source `"delegation"` or `"commission"`
- [ ] `delegatorHistory()`
- [ ] `cDeposit()` / `cWithdraw()` exchange actions - deposit into / withdraw from staking
- [ ] `tokenDelegate()` exchange action - delegate/undelegate to a validator

### Borrow/Lend

- [ ] `borrowLendUserState()` - borrow/supply basis and value per token
- [ ] `borrowLendReserveState()` - rates, utilization, balance, LTV, oracle price
- [ ] `allBorrowLendReserveStates()`

### Account

- [ ] `extraAgents()` - agent names, addresses, validity timestamps. Distinct from the
      already-implemented `approvedBuilders()`.
- [ ] `alignedQuoteTokenInfo()` - isAligned, firstAlignedTime, evmMintedSupply, dailyAmountOwed
- [ ] `queryUserToMultiSigSigners()`

---

## Medium Priority - Error Handling

- [ ] Specific error codes per failure type
- [ ] Rate-limit detection and backoff (now that `reserveRequestWeight` exists, a 429 has two
      possible responses: back off, or buy weight)
- [ ] Retry logic for transient failures
- [ ] Better error messages with suggested fixes

---

## Low Priority - Specialized Features

### Multi-Signature

- [ ] `convertToMultiSigUser()`, `multiSig()`
- [ ] `signMultiSigAction()`, `signMultiSigUserSignedActionPayload()`,
      `signMultiSigL1ActionPayload()`, `addMultiSigTypes()`, `addMultiSigFields()`

### Spot Token Deployment (10 actions)

- [ ] `spotDeployRegisterToken()`, `spotDeployUserGenesis()`, `spotDeployEnableFreezePrivilege()`,
      `spotDeployFreezeUser()`, `spotDeployRevokeFreezePrivilege()`, `spotDeployEnableQuoteToken()`,
      `spotDeployGenesis()`, `spotDeployRegisterSpot()`, `spotDeployRegisterHyperliquidity()`,
      `spotDeploySetDeployerTradingFeeShare()`

### Perp Deployment

- [ ] `perpDeployRegisterAsset()`, `perpDeploySetOracle()`

### Validator Operations

- [ ] `cValidatorRegister()`, `cValidatorChangeProfile()`, `cValidatorUnregister()`,
      `cSignerUnjailSelf()`, `cSignerJailSelf()`
- [ ] `validatorL1Stream()` - vote on risk-free rate for an aligned quote asset
- [ ] `authorizeAqav2Role()` - AQAv2 role authorization

---

## Signing & Conversion Utilities

- [x] `signL1Action()`, `signUserSignedAction()`, `actionHash()`, `constructPhantomAgent()`,
      `l1Payload()`, `userSignedPayload()`
- [x] `floatToWire()`, `floatToUsdInt()`, `floatToInt()`, `orderRequestToOrderWire()`,
      `orderWiresToOrderAction()`, `getTimestampMs()`
- [x] `Cloid`: `fromInt()`, `fromStr()`, `toRaw()`, `validate()`
- [ ] Per-action signing helpers: `signUsdTransferAction()`, `signSpotTransferAction()`,
      `signWithdrawFromBridgeAction()`, `signUsdClassTransferAction()`, `signSendAssetAction()`,
      `signUserDexAbstractionAction()`, `signUserSetAbstractionAction()`,
      `signConvertToMultiSigUserAction()`, `signAgent()`, `signApproveBuilderFee()`,
      `signTokenDelegateAction()`
- [ ] `orderTypeToWire()`, `signInner()`
- [ ] `recoverAgentOrUserFromL1Action()`, `recoverUserFromUserSignedAction()`
- [ ] `floatToIntForHashing()`, `addressToBytes()`

### Python SDK Naming Parity

- [ ] `bulkModifyOrdersNew()` - alias for our `bulkModifyOrders()`
- [ ] `setPerpMeta()` - we have a private `setPerpMeta()`; Python exposes it. Either make ours
      public or document `registerPerpMeta()` as the equivalent.
- [ ] `getDex()` - extract dex prefix from a coin like `xyz:NVDA`
- [ ] `remapCoinSubscription()` - WS subscription coin remapping
- [ ] `spotDeployTokenActionInner()`, `cSignerInner()`

---

## Infrastructure & Quality

### Build & Packaging

- [ ] CMake install targets, pkg-config file, `FindHyperliquid.cmake`
- [ ] Shared library build (.so/.dylib/.dll)
- [ ] Version numbering and API versioning
- [ ] Conan recipe, vcpkg port

### Testing

- [x] Test targets now force `-UNDEBUG`. The build configures as `Release`, which defines
      `NDEBUG` and compiled every `assert()` in the suite to nothing — the tests were passing
      vacuously and could not fail. Keep this flag on any new test target, or write tests that
      don't rely on `assert`.
- [ ] Unit tests: Keccak-256, ECDSA, EIP-712, signing, float conversion/precision
- [ ] Integration tests against testnet
- [ ] Mock HTTP server for network-free tests
- [ ] CI (GitHub Actions) and coverage reporting

### Documentation

- [ ] Doxygen API reference, architecture docs, Python migration guide
- [ ] Advanced examples: market making, TWAP execution, portfolio rebalancing,
      WebSocket real-time data, multi-account
- [ ] Security best practices, troubleshooting

### Performance

- [ ] HTTP connection pooling
- [ ] Metadata caching improvements
- [ ] Async support (C++20 coroutines)
- [ ] Zero-copy message parsing (matters most for `fastAssetCtxs` and `l2Book`)

### Platform

- [ ] Windows MSVC (currently macOS/Linux)
- [ ] ARM, cross-compilation
- [ ] clang-tidy, sanitizers (ASan/UBSan/TSan)

---

## Feature Comparison Matrix

| Category | Hyperliquid API | C++ SDK | Status |
|----------|-----------------|---------|--------|
| **Core Trading** |
| Limit / Market Orders | ✅ | ✅ | Complete |
| Cancel / Modify / Bulk | ✅ | ✅ | Complete |
| Schedule Cancel | ✅ | ✅ | Complete |
| TWAP Orders | ✅ | ✅ | Complete |
| **Account** |
| Leverage / Isolated Margin | ✅ | ✅ | Complete |
| Transfers (USD/Spot) | ✅ | ✅ | Complete |
| USD Class Transfer | ✅ | ✅ | Complete |
| Vault Transfers | ✅ | ❌ | TODO |
| Sub-Accounts | ✅ | ❌ | TODO |
| Bridge Withdrawal | ✅ | ❌ | TODO |
| **Market Data** |
| User / Spot State | ✅ | ✅ | Complete |
| Open + Historical Orders | ✅ | ✅ | Complete |
| Fills (+ by time, TWAP slices) | ✅ | ✅ | Complete |
| L2 Book | ✅ | ✅ | Complete |
| Candles | ✅ | ✅ | Complete |
| All Mids | ✅ | ✅ | Complete |
| Funding History | ✅ | ✅ | Complete |
| Predicted Fundings | ✅ | ❌ | TODO |
| User Fees | ✅ | ✅ | Complete |
| Portfolio | ✅ | ✅ | Complete |
| Vault Details | ✅ | ✅ | Complete |
| Token Details | ✅ | ❌ | TODO |
| Perp Dex Limits / Status | ✅ | ❌ | TODO |
| Margin Tables | ✅ | ❌ | TODO |
| Borrow/Lend | ✅ | ❌ | TODO |
| **Real-Time** |
| WebSocket (24 subscriptions) | ✅ | ❌ | TODO |
| WebSocket POST requests | ✅ | ❌ | TODO |
| **Advanced** |
| Agents / Referrals / Builders | ✅ | ⚠️ | Read-only (`approvedBuilders`, `maxBuilderFee`) |
| Dex Abstraction | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| Staking | ✅ | ❌ | TODO |
| Prediction Markets / Outcomes | ✅ | ❌ | TODO |
| Multi-Sig | ✅ | ❌ | TODO |
| Token / Perp Deployment | ✅ | ❌ | TODO |
| Validators | ✅ | ❌ | TODO |

---

## Roadmap

### Phase 1: WebSocket
Everything real-time is blocked on this. Start with the manager + `l2Book`, `trades`, `allMids`,
`userFills`, `orderUpdates`; add the remaining 19 subscriptions after. Note `fastAssetCtxs` needs
raw-DEFLATE decompression, and `webData2` no longer exists.

### Phase 2: Order Types & Rate Limits — DONE
~~TWAP orders~~, ~~`reserveRequestWeight`~~, ~~`topUpIsolatedOnlyMargin`~~ all done.

### Phase 3: Account Management
Sub-accounts, vault transfers, `usdClassTransfer`, bridge withdrawal, agents, referrals.

### Phase 4: New Surfaces
Staking, borrow/lend, prediction-market outcomes, the newly-documented perp queries
(`predictedFundings`, `allPerpMetas`, `perpDexLimits`, annotations), margin-table modeling.

### Phase 5: Specialized
Multi-sig, validators, token/perp deployment.

### Phase 6: Production Readiness
Tests, CI, packaging, docs, cross-platform.

---

## Contributing

1. **Reference the docs first** — this file was rebuilt from the official Info/Exchange/WebSocket
   endpoint pages. The Python SDK at `/hyperliquid-python-sdk/` lags the API in places (it still
   uses `webData2`), so treat the docs as the source of truth and Python as a signing reference.
2. Follow existing C++ SDK patterns and naming.
3. Add tests for new functionality.
4. Add or update an example.
5. Update README.md and inline docs.
6. Use `std::optional` / `std::variant` where appropriate.
7. Throw exceptions with helpful messages.

---

## Notes

- **Biggest gaps, in order**: WebSocket, TWAP orders, staking, borrow/lend, sub-accounts.
- **Silent drift to watch**: the docs added `marginTables` / `marginMode` / `growthMode` to `meta`
  and deprecated `onlyIsolated`. Our `Meta` type ignores all of it, so HIP-3 dex assets are
  under-modeled today.
- **`webData2` is retired** — anything targeting it should target `webData3`.

---

**Last Updated**: 2026-07-14
**Rebuilt from**: Hyperliquid docs (Info, Exchange, WebSocket endpoints) via MCP
**C++ SDK Version**: 1.0.0
