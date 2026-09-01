# Hyperliquid C++ SDK - TODO List

## Feature Comparison Matrix

| Category | Hyperliquid API | C++ SDK | Status |
|----------|-----------------|---------|--------|
| **Core Trading** |
| Limit / Market Orders | ✅ | ✅ | Complete |
| Cancel / Modify / Bulk | ✅ | ✅ | Complete (`modifyOrder` routes through `batchModify`) |
| Schedule Cancel | ✅ | ✅ | Complete |
| TWAP Orders | ✅ | ✅ | Complete |
| **Account** |
| Leverage / Isolated Margin | ✅ | ✅ | Complete |
| Transfers (USD/Spot) | ✅ | ✅ | Complete |
| USD Class Transfer | ✅ | ✅ | Complete |
| Vault Transfers | ✅ | ❌ | TODO |
| Sub-Accounts | ✅ | ✅ | Complete (create + USD/spot transfer; master-signed, vault excluded) |
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
| Borrow/Lend | ✅ | ✅ | Complete (no HTTP action exists; writes are CoreWriter-only) |
| **Real-Time** |
| WebSocket (24 subscriptions) | ✅ | ✅ | Complete (raw JSON callbacks; no typed structs) |
| WebSocket POST requests | ✅ | ❌ | TODO |
| **Advanced** |
| Agents / Referrals / Builders | ✅ | ✅ | Complete (`approveAgent`, `approveBuilderFee`, `setReferrer` + read queries) |
| Dex Abstraction | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| Staking | ✅ | ✅ | Complete (`cDeposit`, `cWithdraw`, `tokenDelegate` + read queries) |
| Prediction Markets / Outcomes | ✅ | ⚠️ | Read-only (queries done, actions missing) |
| Multi-Sig | ✅ | ✅ | Complete |
| Token / Perp Deployment | ✅ | ❌ | TODO (14 `perpDeploy` variants, 12 `spotDeploy`, HIP-4 outcomes) |
| Validators | ✅ | ❌ | TODO |

---

## Pending Tasks

### WebSocket

- [ ] WebSocket POST requests — `{"method": "post", "id": n, "request": {"type": "info"|"action", "payload": {...}}}`. Explorer requests unsupported over WS.
- [ ] Validate `WsUserEvent`'s funding/liquidation/nonUserCancel arms and most ledger delta variants against live data — only `fills`, `send`, `subAccountTransfer` observed so far.

### Exchange Actions

- [ ] `agentSendAsset()`
- [ ] `sendToEvmWithData()` — HyperCore → HyperEVM with calldata
- [ ] `withdraw3()` — bridge withdrawal
- [ ] `vaultTransfer()` — deposit/withdraw from a vault
- [ ] `hip3LiquidatorTransfer()` — deposit/withdraw from an HIP-3 DEX backstop liquidator
- [ ] `claimRewards()`
- [ ] `userDexAbstraction()`, `agentEnableDexAbstraction()`
- [ ] `userSetAbstraction()` (`"unifiedAccount"`/`"portfolioMargin"`/`"disabled"`), `agentSetAbstraction()` (`"u"`/`"p"`/`"i"`)
- [ ] `useBigBlocks()` — big-blocks mode
- [ ] `gossipPriorityBid()` — Python SDK only, no public doc page

### Info Queries

- [ ] `userOutcome()` exchange action — split/merge outcome, merge question, negate outcome
- [ ] `alignedQuoteTokenInfo()` — no request shape has worked on mainnet or testnet yet; retry once an aligned quote asset is deployed

### Error Handling

- [ ] Specific error codes per failure type
- [ ] Rate-limit detection and backoff
- [ ] Retry logic for transient failures
- [ ] Better error messages with suggested fixes

### Specialized Features

- [ ] Spot Token Deployment (12 `spotDeploy` variants): `registerToken2`, `userGenesis`, `genesis`, `registerSpot`, `registerHyperliquidity`, `setDeployerTradingFeeShare`, `enableQuoteToken`, `enableAlignedQuoteToken`, `disableQuoteToken`, `disableAlignedQuoteToken`, `setTokenAnnotation`, `setDeployerLabel`
- [ ] Prediction-Market Deployment (HIP-4): `activateOutcomeDeployer()`; `outcomeDeploy` variants `registerStandaloneOutcomeFromTemplate`, `registerQuestionFromTemplate`, `registerAndAssociateNamedOutcomeFromTemplate`, `settleOutcome`, `settleQuestion2`, `setSubDeployers`
- [ ] Perp Deployment (14 `perpDeploy` variants): `registerAsset2`, `registerAsset`, `setOracle`, `setFundingMultipliers`, `setFundingInterestRates`, `setMarginTableIds`, `setMarginModes`, `setOpenInterestCaps`, `setDeployerFees`, `setPerpAnnotation`, `haltTrading`, `disableDex`, `setFeeRecipient`, `setSubDeployers`
- [ ] Validator Operations: `cValidatorRegister()`, `cValidatorChangeProfile()`, `cValidatorUnregister()` (action `CValidatorAction`); `cSignerJailSelf()`, `cSignerUnjailSelf()` (action `CSignerAction`) — none have a public doc page, Python SDK only; plus `validatorL1Stream()`, `authorizeAqav2Role()`

### Signing & Conversion Utilities

- [ ] Per-action signing helpers: `signUsdTransferAction()`, `signSpotTransferAction()`, `signWithdrawFromBridgeAction()`, `signUsdClassTransferAction()`, `signSendAssetAction()`, `signUserDexAbstractionAction()`, `signUserSetAbstractionAction()`, `signConvertToMultiSigUserAction()`, `signAgent()`, `signApproveBuilderFee()`, `signTokenDelegateAction()`
- [ ] `orderTypeToWire()`, `signInner()`
- [ ] `recoverAgentOrUserFromL1Action()`, `recoverUserFromUserSignedAction()`
- [ ] `floatToIntForHashing()`, `addressToBytes()`
- [ ] Python SDK naming parity: `bulkModifyOrdersNew()` alias, public `setPerpMeta()` (or document `registerPerpMeta()` as the equivalent), `getDex()`, `remapCoinSubscription()`, `spotDeployTokenActionInner()`, `cSignerInner()`

### Infrastructure & Quality

- [ ] CMake install targets, pkg-config file, `FindHyperliquid.cmake`
- [ ] Shared library build (.so/.dylib/.dll)
- [ ] Version numbering and API versioning
- [ ] Conan recipe, vcpkg port
- [ ] Unit tests: Keccak-256, ECDSA, EIP-712, signing, float conversion/precision
- [ ] Integration tests against testnet
- [ ] Mock HTTP server for network-free tests
- [ ] CI (GitHub Actions) and coverage reporting
- [ ] Doxygen API reference, architecture docs, Python migration guide
- [ ] Advanced examples: market making, TWAP execution, portfolio rebalancing, WebSocket real-time data, multi-account
- [ ] Security best practices, troubleshooting
- [ ] HTTP connection pooling
- [ ] Metadata caching improvements
- [ ] Async support (C++20 coroutines)
- [ ] Zero-copy message parsing (matters most for `fastAssetCtxs` and `l2Book`)
- [ ] Windows MSVC (currently macOS/Linux)
- [ ] ARM, cross-compilation
- [ ] clang-tidy, sanitizers (ASan/UBSan/TSan)
