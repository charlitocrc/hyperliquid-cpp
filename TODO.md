# Hyperliquid C++ SDK - TODO

## Feature Comparison Matrix

| Category | Hyperliquid API | C++ SDK | Notes |
|----------|-----------------|---------|-------|
| **Core Trading** |
| Limit / Market Orders | ✅ | ✅ | |
| Cancel / Modify / Bulk | ✅ | ✅ | by oid or cloid |
| Schedule Cancel | ✅ | ✅ | |
| TWAP Orders | ✅ | ✅ | |
| **Account** |
| Leverage / Isolated Margin | ✅ | ✅ | |
| Transfers (USD / Spot / Class / `sendAsset`) | ✅ | ✅ | |
| Vault Transfers | ✅ | ✅ | takes plain USD, not the Python SDK's wire int |
| Sub-Accounts | ✅ | ✅ | |
| Bridge Withdrawal | ✅ | ✅ | |
| **Market Data** |
| State, Orders, Fills, Book, Candles, Mids | ✅ | ✅ | |
| Funding, Fees, Portfolio, Vaults, Tokens | ✅ | ✅ | |
| Perp Dex Limits / Status, Margin Tables | ✅ | ✅ | |
| Borrow/Lend | ✅ | ✅ | read-only; writes are CoreWriter-only |
| **Real-Time** |
| WebSocket Subscriptions | ✅ | ✅ | 24 types, typed structs |
| WebSocket POST Requests | ✅ | ✅ | |
| **Advanced** |
| Agents / Referrals / Builders | ✅ | ✅ | |
| Staking | ✅ | ✅ | |
| Multi-Sig | ✅ | ✅ | |
| Prediction Markets (HIP-4) | ✅ | ✅ | trading, `userOutcome`, deployer actions, queries |
| Dex / User Abstraction | ✅ | ⚠️ | queries only |
| Token / Perp Deployment | ✅ | ❌ | |
| Validators | ✅ | ❌ | |

## Missing

### Exchange actions
- [ ] `agentSendAsset()`, `sendToEvmWithData()`, `hip3LiquidatorTransfer()`, `claimRewards()`
- [ ] `userDexAbstraction()`, `agentEnableDexAbstraction()`, `userSetAbstraction()`, `agentSetAbstraction()` (queries exist, actions don't)
- [ ] `useBigBlocks()`, `gossipPriorityBid()` (Python SDK only)
- [ ] `marketClose()` for spot/outcome balances (reads perp positions only)
- [ ] Spot deploy (12 `spotDeploy` variants), perp deploy (14 `perpDeploy` variants)
- [ ] Validator ops: `cValidator*`, `cSigner*` (Python SDK only), `authorizeAqav2Role()`

### Info queries
- [ ] `alignedQuoteTokenInfo()`: no request shape works yet

### Unverified against live data
- [ ] Outcome tick: `roundOutcomePrice()` caps at 5 decimals (inferred from live books); test a 6-decimal price on testnet
- [ ] Outcome deployer actions: key order pinned to docs only; needs one testnet deploy
- [ ] `userOutcome` with a subaccount: sent with `vaultAddress`, unconfirmed
- [ ] `outcomeMetaUpdates`: `outcomeSettled`/`outcomeCreated` confirmed live; `questionSettled`/`questionUpdated` unseen
- [ ] `WsUserEvent`: only `fills`, `send`, `subAccountTransfer` seen live
- [ ] User-signed actions (`withdraw3`, `usdSend`, `spotSend`) with a vault configured: does the exchange read `vaultAddress`?

### Python SDK parity
- [ ] Per-action signing helpers (`signUsdTransferAction()` etc.), `orderTypeToWire()`, `signInner()`
- [ ] `recoverAgentOrUserFromL1Action()`, `recoverUserFromUserSignedAction()`, `floatToIntForHashing()`, `addressToBytes()`
- [ ] `bulkModifyOrdersNew()`, `getDex()`, `remapCoinSubscription()`

### Infrastructure
- [ ] Error handling: typed error codes, rate-limit backoff
- [ ] Testnet integration tests, CI
- [ ] Shared library build, pkg-config / `FindHyperliquid.cmake`
- [ ] Windows MSVC
