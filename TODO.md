# Hyperliquid C++ SDK - TODO List

This document tracks features from the Python SDK that need to be implemented to achieve full parity.

## Status Summary

**Current Implementation:**
- ✅ Basic order placement (limit orders)
- ✅ Order cancellation (by OID, by CLOID, bulk)
- ✅ Order modification (single and bulk)
- ✅ Market orders (open, close)
- ✅ Transfers (USD, spot)
- ✅ Leverage management
- ✅ Basic Info queries (userState, openOrders, allMids, userFills, meta, spotMeta, l2Snapshot, queryOrderByOid)
- ✅ EIP-712 signing infrastructure
- ✅ Wallet management with ECDSA secp256k1
- ✅ 4 working examples

**Missing Features:** ~90+ methods and WebSocket support

---

## High Priority - Core Trading Features

### Exchange Class - Advanced Order Management

- [x] `scheduleCancel()` - Schedule future cancel of all orders for a coin
- [x] `queryOrderByCloid()` - Query order status by client order ID (also available in Info class)

### Info Class - Essential Queries

- [x] `spotUserState()` - Get spot trading details for user
- [x] `frontendOpenOrders()` - Get open orders with additional frontend info
- [ ] `historicalOrders()` - Get last 2000 historical orders for user
- [x] `queryOrderByCloid()` - Query order by client order ID
- [x] `userFillsByTime()` - Get fills by time range with aggregation
- [ ] `userFees()` - Get user's fee schedule and volume tier
- [ ] `fundingHistory()` - Get funding rate history for a coin
- [ ] `userFundingHistory()` - Get user's funding payment history
- [ ] `candlesSnapshot()` - Get candle/OHLCV data for charting

### Error Handling Improvements

- [ ] Add specific error codes for different failure types
- [ ] Better error messages with suggested fixes
- [ ] Retry logic for transient failures
- [ ] Rate limiting detection and backoff

---

## Medium Priority - Account Management

### Exchange Class - Margin & Account

- [ ] `updateIsolatedMargin()` - Adjust isolated margin amount for position
- [ ] `setExpiresAfter()` - Set expiration time for actions (partial implementation exists)

### Exchange Class - Sub-Accounts

- [ ] `createSubAccount()` - Create new sub-account under main account
- [ ] `subAccountTransfer()` - Transfer USD between main and sub-accounts
- [ ] `subAccountSpotTransfer()` - Transfer spot tokens to/from sub-accounts

### Exchange Class - Asset Movements

- [ ] `usdClassTransfer()` - Transfer USD between spot and perp classes
- [ ] `sendAsset()` - Send assets between different dexes
- [ ] `vaultUsdTransfer()` - Transfer USD to/from vault contracts
- [ ] `withdrawFromBridge()` - Withdraw assets from L1/L2 bridge

### Info Class - Account Queries

- [ ] `querySubAccounts()` - Get list of sub-accounts for user
- [ ] `queryReferralState()` - Get referral code and statistics
- [ ] `extraAgents()` - Get approved agent addresses
- [ ] `userRole()` - Get user role/account type
- [ ] `userRateLimit()` - Get API rate limit configuration
- [ ] `portfolio()` - Get comprehensive portfolio performance data
- [ ] `userNonFundingLedgerUpdates()` - Get deposits, withdrawals, transfers

---

## Medium Priority - Advanced Trading Features

### Exchange Class - Referrals & Agents

- [ ] `setReferrer()` - Set referral code for fee discounts
- [ ] `approveAgent()` - Approve agent address for trading
- [ ] `approveBuilderFee()` - Approve builder fee structure

### Exchange Class - Abstraction Features

- [ ] `userDexAbstraction()` - Enable/disable dex abstraction mode
- [ ] `agentEnableDexAbstraction()` - Enable dex abstraction for agent

### Info Class - Advanced Queries

- [ ] `userTwapSliceFills()` - Get TWAP slice fills (last 2000)
- [ ] `userVaultEquities()` - Get vault equity positions
- [ ] `queryUserDexAbstractionState()` - Get dex abstraction state

---

## Low Priority - Specialized Features

### Exchange Class - Multi-Signature Support

- [ ] `convertToMultiSigUser()` - Convert account to multi-sig
- [ ] `multiSig()` - Execute multi-sig action with multiple signers

### Info Class - Multi-Sig Queries

- [ ] `queryUserToMultiSigSigners()` - Get multi-sig signers for user

### Signing Utilities - Multi-Sig

- [ ] `signMultiSigAction()` - Sign multi-sig envelope
- [ ] `signMultiSigUserSignedActionPayload()` - Sign multi-sig user actions
- [ ] `signMultiSigL1ActionPayload()` - Sign multi-sig L1 actions
- [ ] `addMultiSigTypes()` - Add multi-sig EIP-712 types
- [ ] `addMultiSigFields()` - Add multi-sig fields to action

---

## Low Priority - Spot Token Deployment

### Exchange Class - Spot Deployment (10 methods)

- [ ] `spotDeployRegisterToken()` - Register new spot token
- [ ] `spotDeployUserGenesis()` - Set initial token holders and amounts
- [ ] `spotDeployEnableFreezePrivilege()` - Enable token freeze capability
- [ ] `spotDeployFreezeUser()` - Freeze user's tokens
- [ ] `spotDeployRevokeFreezePrivilege()` - Revoke freeze capability
- [ ] `spotDeployEnableQuoteToken()` - Enable token as quote currency
- [ ] `spotDeployGenesis()` - Initialize token supply on-chain
- [ ] `spotDeployRegisterSpot()` - Register spot trading pair
- [ ] `spotDeployRegisterHyperliquidity()` - Register hyperliquidity AMM
- [ ] `spotDeploySetDeployerTradingFeeShare()` - Set fee share for deployer

### Info Class - Spot Deployment Queries

- [ ] `querySpotDeployAuctionStatus()` - Get spot deployment auction state

---

## Low Priority - Perpetual Deployment

### Exchange Class - Perp Deployment (2 methods)

- [ ] `perpDeployRegisterAsset()` - Register new perpetual asset
- [ ] `perpDeploySetOracle()` - Update oracle prices for perp

### Info Class - Perp Deployment Queries

- [ ] `metaAndAssetCtxs()` - Get meta with asset contexts
- [ ] `spotMetaAndAssetCtxs()` - Get spot meta with asset contexts
- [ ] `perpDexs()` - Get available perp dexes
- [ ] `queryPerpDeployAuctionStatus()` - Get perp deployment auction status

---

## Low Priority - Validator & Staking

### Exchange Class - Validator Operations (4 methods)

- [ ] `cValidatorRegister()` - Register as validator
- [ ] `cValidatorChangeProfile()` - Update validator profile
- [ ] `cValidatorUnregister()` - Unregister validator
- [ ] `cSignerUnjailSelf()` - Unjail validator signer
- [ ] `cSignerJailSelf()` - Jail validator signer

### Exchange Class - Token Delegation

- [ ] `tokenDelegate()` - Delegate tokens to validator for staking

### Info Class - Staking Queries (4 methods)

- [ ] `userStakingSummary()` - Get staking summary for user
- [ ] `userStakingDelegations()` - Get active delegations
- [ ] `userStakingRewards()` - Get staking rewards history
- [ ] `delegatorHistory()` - Get comprehensive delegation history

---

## Low Priority - Utility Methods

### Exchange Class - Miscellaneous

- [ ] `useBigBlocks()` - Enable/disable big blocks mode for trading
- [ ] `noop()` - No-operation action (for testing)

### Signing Utilities - Additional Signing Functions

- [ ] `signUserSignedAction()` - Sign user-signed actions (non-L1)
- [ ] `signUsdTransferAction()` - Specialized USD transfer signing
- [ ] `signSpotTransferAction()` - Specialized spot transfer signing
- [ ] `signWithdrawFromBridgeAction()` - Specialized bridge withdrawal signing
- [ ] `signUsdClassTransferAction()` - Specialized USD class transfer signing
- [ ] `signSendAssetAction()` - Specialized asset send signing
- [ ] `signUserDexAbstractionAction()` - Specialized dex abstraction signing
- [ ] `signConvertToMultiSigUserAction()` - Specialized multi-sig conversion signing
- [ ] `signAgent()` - Specialized agent approval signing
- [ ] `signApproveBuilderFee()` - Specialized builder fee signing
- [ ] `signTokenDelegateAction()` - Specialized token delegation signing

### Signing Utilities - Recovery Functions

- [ ] `recoverAgentOrUserFromL1Action()` - Recover signer address from L1 action
- [ ] `recoverUserFromUserSignedAction()` - Recover signer from user action

### Conversion Utilities

- [ ] `floatToIntForHashing()` - Convert to 8-decimal int for hashing
- [ ] `floatToUsdInt()` - Convert to 6-decimal USD int
- [ ] `floatToInt()` - Generic float to int with precision
- [ ] `addressToBytes()` - Convert hex address to bytes

---

## Critical Priority - WebSocket Support

### WebSocket Infrastructure

- [ ] `WebSocketManager` class - Real-time data streaming client
  - [ ] Connection management (connect, disconnect, reconnect)
  - [ ] Automatic ping/pong keepalive
  - [ ] Subscription ID tracking
  - [ ] Message routing to callbacks
  - [ ] Thread-safe operation
  - [ ] Connection queuing for pending subscriptions

### WebSocket Subscriptions (13 types)

- [ ] `allMids` - Subscribe to all mid prices
- [ ] `l2Book` - Subscribe to order book updates for coin
- [ ] `trades` - Subscribe to trade updates for coin
- [ ] `bbo` - Subscribe to best bid/offer for coin
- [ ] `candle` - Subscribe to candlestick data at interval
- [ ] `userEvents` - Subscribe to user-specific events
- [ ] `userFills` - Subscribe to user fills/executions
- [ ] `userFundings` - Subscribe to user funding payments
- [ ] `userNonFundingLedgerUpdates` - Subscribe to account ledger changes
- [ ] `orderUpdates` - Subscribe to order status updates
- [ ] `webData2` - Subscribe to generic web data
- [ ] `activeAssetCtx` - Subscribe to active asset context updates
- [ ] `activeAssetData` - Subscribe to active asset data for user/coin

### WebSocket Methods

- [ ] `subscribe()` - Subscribe to real-time data channel
- [ ] `unsubscribe()` - Unsubscribe from channel
- [ ] `disconnectWebsocket()` - Close WebSocket connection cleanly

### WebSocket Message Types (Type Definitions)

- [ ] `AllMidsMsg` - All mid prices message structure
- [ ] `L2BookMsg` - Order book message structure
- [ ] `TradesMsg` - Trades message structure
- [ ] `BboMsg` - Best bid/offer message structure
- [ ] `UserEventsMsg` - User events message structure
- [ ] `UserFillsMsg` - User fills message structure
- [ ] `CandleMsg` - Candle data message structure
- [ ] `ActiveAssetCtxMsg` - Asset context update structure
- [ ] `ActiveAssetDataMsg` - Active asset data structure
- [ ] 5+ additional message type definitions

---

## Infrastructure & Quality

### Build & Packaging

- [ ] Add CMake install targets for system-wide installation
- [ ] Create pkg-config file for easy linking
- [ ] Add CMake find module (FindHyperliquid.cmake)
- [ ] Support building as shared library (.so/.dylib/.dll)
- [ ] Add version numbering and API versioning
- [ ] Create Conan package recipe
- [ ] Create vcpkg port

### Testing Framework

- [ ] Unit tests for crypto utilities (Keccak-256, ECDSA, EIP-712)
- [ ] Unit tests for signing logic
- [ ] Unit tests for float conversion and precision
- [ ] Integration tests with testnet
- [ ] Mock HTTP server for testing without network
- [ ] Test fixtures for common scenarios
- [ ] Continuous integration (GitHub Actions)
- [ ] Code coverage reporting

### Documentation

- [ ] API reference documentation (Doxygen)
- [ ] Architecture documentation
- [ ] Migration guide from Python SDK
- [ ] Advanced examples:
  - [ ] Market making bot example
  - [ ] TWAP execution example
  - [ ] Portfolio rebalancing example
  - [ ] WebSocket real-time data example
  - [ ] Multi-account management example
- [ ] Performance benchmarking guide
- [ ] Security best practices guide
- [ ] Troubleshooting guide

### Performance Optimizations

- [ ] Connection pooling for HTTP requests
- [ ] Request batching for bulk operations
- [ ] Metadata caching improvements
- [ ] Memory pool for frequent allocations
- [ ] Zero-copy message parsing where possible
- [ ] Async/await support (C++20 coroutines)
- [ ] Parallel order processing

### Developer Experience

- [ ] Better error messages with context
- [ ] Debug logging with levels
- [ ] Structured logging support (JSON logs)
- [ ] Request/response interceptors
- [ ] Metrics collection (latency, success rate)
- [ ] Health check utilities
- [ ] Testnet faucet integration

### Platform Support

- [ ] Windows MSVC support (currently macOS/Linux)
- [ ] ARM architecture support
- [ ] Static analysis (clang-tidy)
- [ ] Sanitizer support (ASan, UBSan, TSan)
- [ ] Cross-compilation support

---

## Type System Enhancements

### Additional Type Definitions

- [ ] `PerpAssetCtx` - Perpetual asset context structure
- [ ] `SpotAssetCtx` - Spot asset context structure
- [ ] `AssetInfo` - Detailed asset information
- [ ] `SpotAssetInfo` - Detailed spot asset info
- [ ] `SpotTokenInfo` - Spot token metadata
- [ ] `Leverage` - Cross/isolated leverage info structure
- [ ] `PerpDexSchemaInput` - Perp dex schema input
- [ ] Complete OrderType variants (currently has Limit/Trigger, missing some options)

---

## Feature Comparison Matrix

| Category | Python SDK | C++ SDK | Status |
|----------|-----------|---------|--------|
| **Core Trading** |
| Limit Orders | ✅ | ✅ | Complete |
| Market Orders | ✅ | ✅ | Complete |
| Order Cancellation | ✅ | ✅ | Complete |
| Order Modification | ✅ | ✅ | Complete |
| Bulk Operations | ✅ | ✅ | Complete |
| Schedule Cancel | ✅ | ❌ | TODO |
| **Account Management** |
| Leverage Update | ✅ | ✅ | Complete |
| Isolated Margin | ✅ | ❌ | TODO |
| Transfers (USD/Spot) | ✅ | ✅ | Complete |
| Vault Transfers | ✅ | ❌ | TODO |
| Sub-Accounts | ✅ | ❌ | TODO |
| **Market Data** |
| User State | ✅ | ✅ | Complete |
| Open Orders | ✅ | ✅ | Complete |
| Historical Orders | ✅ | ❌ | TODO |
| User Fills | ✅ | ✅ | Complete |
| Fills by Time | ✅ | ❌ | TODO |
| L2 Book | ✅ | ✅ | Complete |
| Candles | ✅ | ❌ | TODO |
| All Mids | ✅ | ✅ | Complete |
| Funding History | ✅ | ❌ | TODO |
| User Fees | ✅ | ❌ | TODO |
| Portfolio | ✅ | ❌ | TODO |
| **Real-Time Data** |
| WebSocket Support | ✅ | ❌ | TODO |
| Live Order Updates | ✅ | ❌ | TODO |
| Live Fills | ✅ | ❌ | TODO |
| Live Market Data | ✅ | ❌ | TODO |
| **Advanced Features** |
| Multi-Sig | ✅ | ❌ | TODO |
| Referrals | ✅ | ❌ | TODO |
| Agents | ✅ | ❌ | TODO |
| Dex Abstraction | ✅ | ❌ | TODO |
| Validators/Staking | ✅ | ❌ | TODO |
| Token Deployment | ✅ | ❌ | TODO |
| Perp Deployment | ✅ | ❌ | TODO |

---

## Roadmap Suggestion

### Phase 1: Essential Features (Next)
1. WebSocket support (critical for real-time trading)
2. Historical orders and fills by time
3. Candles data for charting
4. User fees and funding history

### Phase 2: Account Management
1. Sub-account creation and transfers
2. Isolated margin management
3. Vault transfers
4. Referral system

### Phase 3: Advanced Trading
1. Dex abstraction
2. Agent management
3. Schedule cancel
4. TWAP slice fills

### Phase 4: Specialized Features
1. Multi-signature support
2. Validator and staking
3. Token deployment
4. Perp deployment

### Phase 5: Production Readiness
1. Comprehensive testing framework
2. Performance optimizations
3. Documentation completion
4. Cross-platform support
5. Packaging and distribution

---

## Contributing

When implementing features from this TODO:

1. **Reference Python SDK**: Always check the Python implementation at `/hyperliquid-python-sdk/`
2. **Maintain Consistency**: Follow existing C++ SDK patterns and naming conventions
3. **Add Tests**: Include unit tests for new functionality
4. **Update Examples**: Add or update examples demonstrating new features
5. **Document**: Update README.md and add inline documentation
6. **Type Safety**: Use strong typing with std::optional, std::variant where appropriate
7. **Error Handling**: Throw appropriate exceptions with helpful messages

---

## Notes

- **Total Methods Missing**: ~90+ methods across Exchange and Info classes
- **WebSocket**: Complete real-time data infrastructure needed
- **Priority**: Focus on high/medium priority items first - they cover 80% of use cases
- **Low Priority**: Specialized features (validators, token deployment) can wait
- **Infrastructure**: Testing and documentation are critical for production use

---

**Last Updated**: 2025-12-27
**C++ SDK Version**: 1.0.0
**Python SDK Version**: Based on latest main branch
