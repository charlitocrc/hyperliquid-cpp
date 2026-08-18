# Hyperliquid C++ SDK - TODO List

Tracks the gap between this SDK and the Hyperliquid API. Sourced from the official
docs (Info / Exchange / WebSocket endpoints) and cross-checked against the request
`type` strings actually emitted by `src/info.cpp` and `src/exchange.cpp`.

## Status Summary

**Implemented today** (verified against source, not aspiration):

- Info (56 request types): `allMids`, `clearinghouseState`, `spotClearinghouseState`, `openOrders`,
  `frontendOpenOrders`, `historicalOrders`, `orderStatus`, `l2Book`, `candleSnapshot`, `meta`,
  `metaAndAssetCtxs`, `spotMeta`, `spotMetaAndAssetCtxs`, `perpDexs`, `perpDeployAuctionStatus`,
  `userFills`, `userFillsByTime`, `userFunding`, `fundingHistory`, `userNonFundingLedgerUpdates`,
  `userFees`, `userRole`, `userRateLimit`, `userTwapSliceFills`, `userVaultEquities`, `vaultDetails`,
  `subAccounts`, `referral`, `portfolio`, `maxBuilderFee`, `approvedBuilders`, `userDexAbstraction`,
  `userAbstraction`, `perpDexLimits`, `perpDexStatus`, `tokenDetails`, `predictedFundings`,
  `perpsAtOpenInterestCap`, `perpCategories`, `perpConciseAnnotations`, `perpAnnotation`,
  `userToMultiSigSigners`, `allPerpMetas`, `activeAssetData`, `spotDeployState`,
  `spotPairDeployAuctionStatus`, `outcomeMeta`, `settledOutcome`, `delegatorSummary`,
  `delegations`, `delegatorRewards`, `delegatorHistory`, `borrowLendUserState`,
  `borrowLendReserveState`, `allBorrowLendReserveStates`, `extraAgents`
- Exchange (25 actions): `order`, `cancel`, `cancelByCloid`, `modify`, `batchModify`, `scheduleCancel`,
  `updateLeverage`, `updateIsolatedMargin`, `topUpIsolatedOnlyMargin`, `usdSend`, `spotSend`,
  `usdClassTransfer`, `sendAsset`, `twapOrder`, `twapCancel`, `reserveRequestWeight`,
  `approveBuilderFee`, `approveAgent`, `noop`, `setReferrer`, `createSubAccount`, `subAccountTransfer`,
  `subAccountSpotTransfer`, `convertToMultiSigUser`, `multiSig`
- Market orders (`marketOpen` / `marketClose`), EIP-712 signing, ECDSA secp256k1 wallet,
  automatic tick/lot rounding, `setExpiresAfter`
- 22 working examples in `examples/` (one needs BUILD_WEBSOCKET)

**Not implemented at all:** vaults, token/perp deployment. Staking, borrow/lend and
prediction-market outcomes are read-only so far: every info query ships, none of the
exchange actions do.

---

## Critical Priority - WebSocket Support

### Infrastructure — DONE

Transport is IXWebSocket, fetched via `FetchContent` behind `option(BUILD_WEBSOCKET)` (default
ON). Not libcurl: curl only speaks `ws`/`wss` when built with `--enable-websockets`, and the
libcurl in the macOS SDK is not — it links and then fails at runtime.

- [x] `WebSocketManager` class in `include/hyperliquid/websocket_manager.hpp` /
      `src/websocket_manager.cpp`
  - [x] Connection management. Reconnect is automatic; every `Open` re-sends **all** active
        subscriptions, because the server keeps no record of them across connections. There is
        deliberately no separate "queued before connect" list like the Python SDK's — that
        design silently loses every subscription after a reconnect.
  - [x] Ping keepalive: `{"method":"ping"}` every 50s from a thread parked on a
        `condition_variable`, which doubles as the stop signal. Not IXWebSocket's
        `setPingInterval`, which sends RFC 6455 frames rather than the documented JSON ping.
  - [x] Subscription id tracking and identifier-based routing to callbacks
  - [x] Thread-safe. Callbacks fire on the network thread with the lock released, so a
        callback may call `subscribe()` without deadlocking.
- [x] `subscribe()` / `unsubscribe()` / `disconnectWebsocket()`, on `WebSocketManager` and
      forwarded from `Info` (which is what the previously-dead `skip_ws` flag now controls)
- [x] `subscriptionToIdentifier()` / `messageToIdentifier()`, plus `start()`, `stop()`,
      `onOpen()`, `onMessage()`, `pingLoop()`
- [x] `setErrorCallback()` / `setUnhandledCallback()` — no Python equivalent. Without them a
      routing mismatch is indistinguishable from "no data".
- [ ] **WebSocket POST requests** - the WS API can carry any info request or signed action
      (`{"method": "post", "id": n, "request": {"type": "info"|"action", "payload": {...}}}`).
      Explorer requests are not supported over WS.

### Subscriptions (24 documented types) — DONE

All 24 are routable. `webData2` is gone — it is `webData3` now.

Routing uses one 24-row table (`kChannels` in `src/websocket_manager.cpp`) driving both the
outbound and inbound identifier, rather than the Python SDK's two parallel if-chains that have
to agree by hand. Pinned by `tests/websocket_identifier_test.cpp`.

Discriminators are only applied where a live message is known to echo the field. The ten types
marked ✓keyed below route per coin/user; the rest route on the subscription type alone, which
can over-deliver to a second subscriber of the same type but can never drop a message. Promote
one to keyed by filling in `sub_key`/`msg_key` once a live payload confirms the field.

- [x] `allMids` (takes optional `dex`; spot mids only ride along with the first perp dex)
- [x] `notification`
- [x] `webData3` ✓keyed by user — replaces the old `webData2`
- [x] `twapStates`
- [x] `clearinghouseState` (per-dex)
- [x] `openOrders` (per-dex)
- [x] `candle` ✓keyed by coin+interval (subscription says `coin`/`interval`, message says `s`/`i`)
- [x] `l2Book` ✓keyed by coin (optional `nSigFigs`, `mantissa`, `fast` pass through untouched)
- [x] `trades` ✓keyed by coin (message is an array; routes on element 0)
- [x] `orderUpdates`
- [x] `userEvents` (inbound channel name is `"user"`, handled)
- [x] `userFills` ✓keyed by user (optional `aggregateByTime`)
- [x] `userFundings` ✓keyed by user
- [x] `userNonFundingLedgerUpdates` ✓keyed by user
- [x] `activeAssetCtx` ✓keyed by coin (channel is `activeSpotAssetCtx` for spot; aliased)
- [x] `activeAssetData` ✓keyed by coin+user (perps only)
- [x] `userTwapSliceFills`
- [x] `userTwapHistory`
- [x] `bbo` ✓keyed by coin
- [x] `spotState` (optional `isPortfolioMargin`)
- [x] `allDexsClearinghouseState`
- [x] `allDexsAssetCtxs`
- [x] `outcomeMetaUpdates` (prediction markets)
- [x] `fastAssetCtxs` — decoded automatically. This is the only subscription whose payload is
      not plain JSON: base64 wrapping **raw DEFLATE** (RFC 1951, no zlib/gzip wrapper), decoded
      base64 → inflate (zlib `windowBits = -15`) → UTF-8 → JSON before it reaches a callback,
      so subscribers see the same shape as every other channel. `decodeFastAssetCtxs()` is also
      public for payloads captured elsewhere; it returns `std::optional` and never throws, since
      it runs on the socket thread against untrusted input. Inflation is capped at 32 MB.
      First message is a snapshot; later messages carry only changed coins, and a coin's
      unchanged fields are omitted from its delta. Pinned against the documented
      payload/decoding pair in `tests/websocket_identifier_test.cpp`.

### Message Types — DONE

In `include/hyperliquid/ws_types.hpp` / `src/ws_types.cpp`, as nlohmann `from_json`
conversions found by ADL. **Strictly opt-in**: callbacks still receive raw `nlohmann::json`
per CLAUDE.md §5, and nothing converts unless you ask for it:

```cpp
ws.subscribe(json{{"type","l2Book"},{"coin","ETH"}}, [](const json& msg) {
    const auto book = msg["data"].get<hyperliquid::WsBook>();
});
```

**The docs are wrong about numeric types, and it matters.** Every price and quantity the
TypeScript definitions declare as `number` is sent as a *decimal string* on the live wire —
verified against the API for `markPx`, `midPx`, `funding`, `openInterest`, `oraclePx`,
`dayNtlVlm`, `prevDayPx`, `accountValue`, `totalNtlPos`, `totalRawUsd`, `totalMarginUsed`,
`withdrawable`, and candle `o`/`c`/`h`/`l`/`v`. A strict `get<double>()` compiles and then
throws on the first real message. Every such field is read through a converter accepting
either form. Fields the docs declare as `string` stay `std::string`, which also preserves
exact decimal precision on the money-carrying fill and order fields.

- [x] `WsTrade`, `WsBook`, `WsBbo`, `WsLevel`, `AllMids`, `Candle`, `Notification`
- [x] `WsUserEvent` variants: `WsFill`, `WsUserFunding`, `WsLiquidation`, `WsNonUserCancel`,
      plus `FillLiquidation` and the `WsUserFills` wrapper
- [x] `WsOrder` / `WsBasicOrder`
- [x] `WsActiveAssetCtx`, `WsActiveSpotAssetCtx`, `PerpsAssetCtx`, `SpotAssetCtx`
- [x] `WsActiveAssetData`, plus `Leverage` (referenced but never defined in the WS docs;
      layout taken from the info endpoint and confirmed live as `{"type":"cross","value":20}`)
- [x] `TwapState`, `WsTwapHistory`, `WsUserTwapHistory`, `WsTwapSliceFill`,
      `WsUserTwapSliceFills`, `TwapStates`
- [x] `WebData3`, `WebData3UserState`, `PerpDexState`, `LeadingVault`
- [x] `ClearinghouseState`, `InnerClearinghouseState`, `MarginSummary`, `AssetPosition`,
      `OpenOrders`
- [x] `WsSpotState`, `SpotState`, `UserBalance`
- [x] `WsAllDexsClearinghouseState`, `WsAllDexsAssetCtxs`
- [x] `WsOutcomeMetaUpdates`, `WsOutcomeMetaUpdate`, `OutcomeSpec`, `OutcomeSideSpec`,
      `QuestionSpec`
- [x] `WsFastAssetCtxs` / `FastAssetCtx` — both fields optional, since after the snapshot each
      message carries only changed coins and only their changed fields
- [x] `WsUserNonFundingLedgerUpdate` and its 12 delta variants as a `std::variant`, dispatched
      on the `type` tag. The three vault deltas (`vaultCreate`/`vaultDeposit`/
      `vaultDistribution`) share one `WsVaultDelta` struct that retains the tag, because they
      share one layout. An unrecognised `type` throws rather than silently decoding as a
      default-constructed deposit.

Two fields stay raw `nlohmann::json` on purpose: `AssetPosition::position` and
`OpenOrders::orders`. The websocket docs name the inner `Position` and `Order` types without
defining them, and inventing a layout from another endpoint risks being silently wrong.

**Union style follows the wire.** `WsUserEvent` and `WsOutcomeMetaUpdate` use
`std::optional` members because those unions are expressed by which key is present.
`WsLedgerUpdate` uses `std::variant` because it carries an explicit `type` discriminator.

- [x] `WsUserFundings` and `WsUserNonFundingLedgerUpdates` envelopes. The docs never define
      these wrappers; the field names were read off live payloads
      (`{fundings, isSnapshot, user}` and `{isSnapshot, nonFundingLedgerUpdates, user}`).

Covered by `tests/ws_types_test.cpp` (34 checks) against real wire shapes, not the documented
ones. **Every type reachable with a real account has now been validated against live mainnet
data**, using `0xcf3f419d…f95f` for the account feeds and `0xa8cde6a1…7275` for TWAP:

`WsBook`, `WsTrade`, `WsBbo`, `Candle`, `AllMids`, `WsActiveAssetCtx`, `WsActiveSpotAssetCtx`,
`WsFastAssetCtxs` (1241 coins), `WsAllDexsAssetCtxs` (10 dexs), `ClearinghouseState`,
`OpenOrders` (109 orders), `WsOrder`, `WsUserEvent`, `Notification`, `WebData3` (10 dex
states), `WsSpotState`, `WsActiveAssetData`, `WsAllDexsClearinghouseState`, `WsUserFills`,
`WsUserFundings` (100 entries), `WsUserNonFundingLedgerUpdates` (58 entries), `TwapStates`,
`WsUserTwapHistory`, `WsUserTwapSliceFills`, plus `InnerClearinghouseState` (175 positions),
`Leverage` and `WsFill` (2000 fills) off the info endpoint. Zero conversion failures.

### Further ways the docs diverge from the wire

Found by running the types against live accounts. Each one broke something before it was fixed:

- **`webData3` carries no top-level `user`** — the address is nested under `userState`. The
  routing table had keyed it on a flat `data["user"]`, so every message matched no subscription
  and was dropped. It now routes on the channel name alone, and `messageToIdentifier()` falls
  back to the bare type whenever a routing field is missing, so this class of mistake
  over-delivers instead of silently dropping. Pinned in `tests/websocket_identifier_test.cpp`.
- **`WsTwapHistory.status.description` does not exist.** The docs mark it required; every live
  status object is `{"status": "activated"}` alone. Requiring it threw on every real message.
  Now optional.
- **`WsTwapHistory` carries an undocumented `twapId`**, the only way to tie an entry back to a
  running TWAP. Now exposed as `twap_id`.
- **TWAP status values beyond the documented four.** The docs list
  activated/terminated/finished/error; `waitingForTrigger` appears live. `status` is a plain
  string, so this needs no change — but do not switch exhaustively on it.
- **A 13th ledger delta type, `send`**, is absent from the docs' list of 12 and is what
  `sendAsset` actually produces. Modelled as `WsSend` (distinct from `WsSpotTransfer`: it also
  carries `sourceDex`, `destinationDex` and `nativeTokenFee`).
- Because that list proved incomplete, an unrecognised delta now decodes to `WsUnknownDelta`
  (type plus raw JSON) rather than throwing. One unknown entry used to make an entire
  account's ledger feed unreadable.
- Undocumented fields ignored harmlessly: `twapId` on fills, `nSamples` on fundings,
  `stopPx`/`trigger` on TWAP states, `abstraction` on the webData3 user state,
  `destinationDex`/`feeToken`/`nativeTokenFee` on transfer deltas.

- [ ] `WsUserEvent`'s funding, liquidation and nonUserCancel arms are still unexercised — only
      the `fills` arm appeared live. Same for most ledger delta variants: only `send` and
      `subAccountTransfer` were observed on the test account.

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

All three are plain L1 actions, but they act on the master account: a
vault/subaccount configured on the Exchange is neither signed over nor sent.
The Python SDK signs them with no vault yet still puts the configured vault in
the envelope, so the signature does not cover the vault it ships with and the
server cannot verify it — we omit it from both instead. Shared with
`setReferrer()` via `actionIgnoresVault()` in `src/exchange.cpp`.

- [x] `createSubAccount()` - action `createSubAccount`; wire `{type, name}`. Response carries
      the new sub-account address, which the transfers below take.
- [x] `subAccountTransfer()` - action `subAccountTransfer`; wire
      `{type, subAccountUser, isDeposit, usd}`, usd in micro-USDC.
- [x] `subAccountSpotTransfer()` - action `subAccountSpotTransfer`; wire
      `{type, subAccountUser, isDeposit, token, amount}`, amount via `floatToWire`
      (Python sends `str(amount)`).
- [x] Vault-exclusion cases in `tests/l1_action_signing_test.cpp`, example in
      `examples/sub_accounts.cpp`

### Agents, Referrals, Builders

- [x] `approveAgent()` - action `approveAgent`. Generates the agent key locally (OpenSSL
      `RAND_bytes`) and returns `{response, agent_key}` like Python. User-signed
      (`HyperliquidTransaction:ApproveAgent`); signed with `agentName: ""` when unnamed but the
      field is omitted from the wire (Python SDK parity). Signatures (named + unnamed) pinned
      against the Python SDK in `tests/approve_agent_test.cpp`. Example in
      `examples/approve_agent.cpp`.
- [x] `approveBuilderFee()` - action `approveBuilderFee`. User-signed
      (`HyperliquidTransaction:ApproveBuilderFee`); `builder` is an EIP-712 `address` field —
      first use of that encoding, signature pinned against the Python SDK in
      `tests/approve_builder_fee_test.cpp`. Example in `examples/approve_builder_fee.cpp`.
- [x] `setReferrer()` - action `setReferrer`; wire `{type, code}`. Plain L1 action, but acts
      on the master account and so ignores a configured vault — see the Sub-Accounts note.
      One shot per account, and only before it has traded. Covered in
      `tests/l1_action_signing_test.cpp`; shown (commented out) in `examples/sub_accounts.cpp`.

### Abstraction

- [ ] `userDexAbstraction()` - action `userDexAbstraction`
- [ ] `agentEnableDexAbstraction()` - action `agentEnableDexAbstraction`
- [ ] `userSetAbstraction()` - action `userSetAbstraction` (`"unifiedAccount"` / `"portfolioMargin"` / `"disabled"`)
- [ ] `agentSetAbstraction()` - action `agentSetAbstraction` (`"u"` / `"p"` / `"i"`)

### Misc

- [x] `noop()` - action `noop`; invalidates a pending nonce. The only action taking a
      caller-supplied nonce (`postL1Action` grew an optional override for it); defaults to a
      generated timestamp, unlike Python where the nonce is required. Signs the configured
      vault like any ordinary L1 action. Covered in `tests/l1_action_signing_test.cpp`.
- [ ] `useBigBlocks()` - big-blocks mode
- [ ] `gossipPriorityBid()` - Python SDK action, no public doc page

---

## High Priority - Missing Info Queries

### Perps (newly surfaced by docs, never tracked here)

- [x] `predictedFundings()` - `predictedFundings`; funding across venues (first perp dex only,
      no dex parameter). Returns `[[coin, [[venue, {fundingRate, nextFundingTime,
      fundingIntervalHours}], ...]], ...]`. Rates are NOT comparable across venues without
      scaling by `fundingIntervalHours` — Hyperliquid is hourly, Binance/Bybit 4- or 8-hourly.
- [x] `perpsAtOpenInterestCap()` - `perpsAtOpenInterestCap`; array of coin names. Takes an
      optional `dex`, omitted when empty (the `meta()` convention, and the API treats `""` and
      absent identically here).
- [x] `perpDexLimits()` - `perpDexLimits`; OI caps and transfer limits for a builder dex.
      `""` is rejected rather than meaning the default dex, so the SDK throws before
      spending a request.
- [x] `perpDexStatus()` - `perpDexStatus`; total net deposit for a dex. Here `""` IS
      meaningful (first perp dex) and is always sent.
- [x] `perpAnnotation()` - `perpAnnotation`; requires `coin`. Returns
      `{category, description, displayName?, keywords?}`, or **null** when the coin has no
      annotation. Annotations exist for builder-dex coins (`flx:BTC`, `xyz:TSLA`); plain
      default-dex coins like `BTC` return null. An empty coin also returns null rather than an
      error, so the SDK throws on it — otherwise a caller bug is indistinguishable from a real
      "unannotated" answer.
- [x] `perpCategories()` - `perpCategories`; `[[coin, category], ...]` across all dexes.
      Live categories: crypto, stocks, commodities, indices. 162 annotated coins as of writing.
- [x] `perpConciseAnnotations()` - `perpConciseAnnotations`; `[[coin, {category, displayName?,
      keywords?}], ...]`. Same coin set as `perpCategories()`, minus the long descriptions.

None of these three are in `hyperliquid_api_docs.md`; request/response shapes above were
established against live mainnet. All five are covered in
`tests/info_perp_deployment_queries_test.cpp` and shown in `examples/dex_and_token_info.cpp`.

- [x] `allPerpMetas()` - `allPerpMetas`; metadata for every dex in one call, index-aligned
      with `perpDexs()` (11 dexes live). **The docs are wrong about the shape**: they show
      `[[meta, assetCtxs], ...]`, but live mainnet returns bare meta objects with no contexts
      at all. Use `metaAndAssetCtxs(dex)` per dex when contexts are needed.
- [x] `activeAssetData()` - `activeAssetData`; leverage, max trade size, available-to-trade
      for one user and coin. Perps only. `maxTradeSzs` and `availableToTrade` are
      `[buy, sell]` pairs. The Python SDK reaches this only over the WebSocket; the info
      endpoint serves it too.

- [x] Extend `Meta` / `AssetInfo` with `marginTables`, `marginTableId`, `marginMode`,
      `growthMode`, `collateralToken`, `isDelisted`. Field set verified against live
      mainnet `meta` for the default dex and two builder dexes: everything past
      `name`/`szDecimals` is optional, and `growthMode` (a **string**, e.g. `"enabled"`,
      not a bool) plus `lastGrowthModeChangeTime` appear only on builder dexes.
      `onlyIsolated` is deprecated by `marginMode` but still sent, so still parsed.
      New `MarginTable` / `MarginTier` types; the wire form is `[id, table]` pairs, so
      the id is folded into `MarginTable::id` during parsing.
      Covered in `tests/info_perp_deployment_queries_test.cpp`.

Not done: no helper resolves an asset's `margin_table_id` to its `MarginTable`, or picks
the tier for a given notional. Callers do the lookup themselves.

### Spot

- [x] `tokenDetails()` - `tokenDetails`; supply, genesis, deployer, deploy gas. Takes the
      34-char onchain `tokenId` (the `SpotTokenInfo::token_id` field), not a 42-char address.
- [x] `spotDeployState()` - `spotDeployState`; requires `user`. Returns the token deploy
      gas auction plus that deployer's in-flight deployments (`states`, empty for an
      address that has not started one).
- [x] `spotPairDeployAuctionStatus()` - `spotPairDeployAuctionStatus`; the separate Dutch
      auction for deploying a pair between two existing tokens. Same shape as
      `perpDeployAuctionStatus`.

### Prediction Markets / Outcomes (entirely new surface)

- [x] `outcomeMeta()` - `outcomeMeta`. The docs show only `{outcomes: [...]}`; live also
      returns `questions`, `deployers` and `feeScale`, and each outcome carries a
      `quoteToken` the docs omit. The `sideSpecs`/`OutcomeSpec`/`QuestionSpec` shapes match
      the `outcomeMetaUpdates` websocket types already in `ws_types.hpp`.
- [x] `settledOutcome()` - `settledOutcome`; takes an outcome index. Returns **null** while
      the outcome is open *and* for an index that does not exist, so a null answer does not
      distinguish the two. Settled entries add an undocumented `question` object for named
      outcomes.
- [ ] `userOutcome()` exchange action - split / merge outcome, merge question, negate outcome

### Staking

Read side done. The four wire types are named after the question, not the answer
(`delegatorSummary`, `delegations`, `delegatorRewards`), so the method-to-type mapping is
pinned in `tests/info_account_queries_test.cpp`.

- [x] `userStakingSummary()` - `delegatorSummary`; delegated, undelegated, pending withdrawals
- [x] `userStakingDelegations()` - `delegations`; per-validator, with locked-until timestamps
- [x] `userStakingRewards()` - `delegatorRewards`; source `"delegation"` or `"commission"`,
      the latter only for validators
- [x] `delegatorHistory()` - `delegatorHistory`; delegate / cDeposit / withdrawal deltas as
      tagged objects. Protocol-generated entries carry an all-zero hash.
- [ ] `cDeposit()` / `cWithdraw()` exchange actions - deposit into / withdraw from staking
- [ ] `tokenDelegate()` exchange action - delegate/undelegate to a validator

### Borrow/Lend

- [x] `borrowLendUserState()` - `borrowLendUserState`; `tokenToState` is
      `[[token, {borrow: {basis, value}, supply: {basis, value}}], ...]`, `basis` being
      principal and `value` principal plus accrued interest. `healthFactor` is null when
      nothing is borrowed.
- [x] `borrowLendReserveState()` - `borrowLendReserveState`; takes a numeric token index.
      Rates, utilization, balance, LTV, oracle price. An `ltv` of `"0.0"` means the token
      is not accepted as collateral (true of USDC today).
- [x] `allBorrowLendReserveStates()` - `[[token, state], ...]`; 5 reserves live.

### Account

- [x] `extraAgents()` - `extraAgents`; agent names, addresses, validity timestamps.
      Distinct from the already-implemented `approvedBuilders()`. **`validUntil` is null**
      for agents that do not expire, though both the docs and the Python SDK type it as an
      int; verified live.
- [ ] `alignedQuoteTokenInfo()` - isAligned, firstAlignedTime, evmMintedSupply, dailyAmountOwed.
      **Not implemented: the request could not be made to work against either network.**
      The December docs snapshot documents it as `{type, token: <index>}`, but that and every
      other spelling tried (`tokens`, `tokenIndex`, `coin`, string token, no params, plural
      type name) return "Failed to deserialize the JSON body into the target type" on both
      mainnet and testnet, while known-good requests on the same connection succeed. The
      current docs page no longer lists it. Likely not served yet — there are no aligned
      quote assets on mainnet. Retry when one is deployed rather than shipping a guess.
- [x] `queryUserToMultiSigSigners()` - `userToMultiSigSigners`; the account's authorized
      users and threshold. Returns **null** for an address that is not a multi-sig account
      (verified live), which is how the multi-sig example checks before signing anything.

---

## Medium Priority - Error Handling

- [ ] Specific error codes per failure type
- [ ] Rate-limit detection and backoff (now that `reserveRequestWeight` exists, a 429 has two
      possible responses: back off, or buy weight)
- [ ] Retry logic for transient failures
- [ ] Better error messages with suggested fixes

---

## Low Priority - Specialized Features

### Multi-Signature — DONE

The docs describe the workflow but publish no wire format, deferring to the Python SDK, so
this follows Python's signing byte for byte. Every signature is pinned against it in
`tests/multi_sig_test.cpp` — the payloads are assembled from pieces (an enriched type list,
an array envelope, a hash of a hash) that a shape-only test would accept while the exchange
rejects them.

- [x] `convertToMultiSigUser()` - action `convertToMultiSigUser`; wire
      `{type, signers, nonce}` where `signers` is a JSON *string*. User-signed
      (`HyperliquidTransaction:ConvertToMultiSigUser`).
- [x] `multiSig()` - action `multiSig`; wire `{type, signatureChainId, signatures,
      payload: {multiSigUser, outerSigner, action}}`. Takes the inner action as
      `ordered_json`, since its key order feeds the msgpack hash.
- [x] `signMultiSigAction()`, `signMultiSigUserSignedActionPayload()`,
      `signMultiSigL1ActionPayload()`, `addMultiSigTypes()`, `addMultiSigFields()`
- [x] `convertToMultiSigUserSigners()` - builds the `signers` string. No Python equivalent;
      it exists because changing an existing signer set has to be hand-assembled as an inner
      action, and the string's exact bytes are what gets signed.
- [x] Example in `examples/multi_sig.cpp` (L1 and user-signed inner actions, signer-set
      changes, converting back to a normal user)

Deliberate divergences from the Python SDK, each one a case where mirroring it produces a
signature the exchange cannot verify:

- **The vault comes from one place.** Python's `multi_sig()` signs over a `vault_address`
  argument but ships the Exchange's configured vault in the envelope; when they differ the
  signature covers a vault the request does not carry. Ours signs and sends the configured
  vault, like every other action, so the argument is gone.
- **`addMultiSigTypes()` throws** when the types carry no `hyperliquidChain` entry. Python
  prints a warning and returns them unchanged, producing a well-formed signature that
  verifies against nothing and fails much later as a rejected action.
- **`convertToMultiSigUser()` validates** before spending a request: at most 10 authorized
  users (the documented cap), threshold within `[1, users]`, no duplicates, addresses
  well-formed. A threshold above the number of distinct signers leaves an account that can
  never sign again.

The `signers` string is built by hand rather than with `json::dump()`, to reproduce Python's
`json.dumps` spacing exactly (`{"authorizedUsers": ["0x..", "0x.."], "threshold": 2}`). It is
signed as an opaque string, so the bytes are the contract.

Two things the caller still owns, both documented on `multiSig()`:

- A **user-signed** inner action must carry its own `signatureChainId` and `hyperliquidChain`.
  Nothing adds them on this path — `signMultiSigUserSignedActionPayload()` deliberately does
  not mutate the action, because the leader hashes the same object afterwards.
- The nonce, vault and `expiresAfter` must be identical across every signer and the leader.

Not ported: Python's `_multi_sig_payload_action`, which rewrites a `userSetAbstraction`
inner action's `abstraction` field to its wire enum. This SDK has no `userSetAbstraction`
yet, and silently rewriting a caller-supplied action would be worse than not having it.
Revisit when the Abstraction actions land.

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
- [ ] Zero-copy message parsing (matters most for `fastAssetCtxs` and `l2Book`). Today every
      websocket message costs one `json::parse` plus one identifier string, and `fastAssetCtxs`
      additionally costs a base64 decode and an inflate into a fresh buffer.

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
| Predicted Fundings | ✅ | ✅ | Complete |
| User Fees | ✅ | ✅ | Complete |
| Portfolio | ✅ | ✅ | Complete |
| Vault Details | ✅ | ✅ | Complete |
| Token Details | ✅ | ✅ | Complete |
| Perp Dex Limits / Status | ✅ | ✅ | Complete |
| Margin Tables | ✅ | ✅ | Complete (parsed into `Meta`; no tier-lookup helper) |
| Borrow/Lend | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| **Real-Time** |
| WebSocket (24 subscriptions) | ✅ | ✅ | Complete (raw JSON callbacks; no typed structs) |
| WebSocket POST requests | ✅ | ❌ | TODO |
| **Advanced** |
| Agents / Referrals / Builders | ✅ | ⚠️ | Partial (`approveAgent`, `approveBuilderFee` + read queries; `setReferrer` missing) |
| Dex Abstraction | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| Staking | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| Prediction Markets / Outcomes | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| Multi-Sig | ✅ | ✅ | Complete |
| Token / Perp Deployment | ✅ | ❌ | TODO |
| Validators | ✅ | ❌ | TODO |

---

## Roadmap

### Phase 1: WebSocket — MOSTLY DONE
Manager and all 24 subscriptions ship, delivering raw `nlohmann::json` to callbacks per
CLAUDE.md §5, including `fastAssetCtxs` base64 + raw-DEFLATE decoding. Still open: WS POST
requests and the typed message structs.

### Phase 2: Order Types & Rate Limits — DONE
~~TWAP orders~~, ~~`reserveRequestWeight`~~, ~~`topUpIsolatedOnlyMargin`~~ all done.

### Phase 3: Account Management
Sub-accounts, vault transfers, `usdClassTransfer`, bridge withdrawal, agents, referrals.

### Phase 4: New Surfaces
Info queries all done — staking, borrow/lend, outcomes, `allPerpMetas`, `activeAssetData`,
`predictedFundings`, `perpDexLimits`, annotations, margin-table modeling. Still open: the
exchange actions behind them (`cDeposit`/`cWithdraw`/`tokenDelegate`, borrow/lend
operations, `userOutcome`).

### Phase 5: Specialized
~~Multi-sig~~ done. Still open: validators, token/perp deployment.

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

- **Biggest gaps, in order**: WS POST requests, then the staking/borrow-lend/outcome
  exchange actions — every read side of those three now ships.
- **Silent drift to watch**: `Meta` is the one API response parsed into a struct rather than
  returned as raw JSON, so any field the exchange adds is dropped silently. `marginTables` /
  `marginMode` / `growthMode` / `collateralToken` are modeled now; re-check the field union
  against a live `meta` when HIP-3 changes land.
- **`webData2` is retired** — anything targeting it should target `webData3`.

---

**Last Updated**: 2026-08-18
**Rebuilt from**: Hyperliquid docs (Info, Exchange, WebSocket endpoints) via MCP
**C++ SDK Version**: 1.0.0
