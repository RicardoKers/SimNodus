// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/managed_chain.hpp"
#include <optional>

namespace simnodus::experimental {
struct ManagedStoreError {
    const char* code;
    std::uint32_t system_code{};
    bool indeterminate = false;
};
using ManagedStoreSave = std::variant<ManagedReceipt, ManagedStoreError>;
using ManagedStoreRead = std::variant<std::shared_ptr<const ManagedChain>, ManagedStoreError>;
class ManagedStore;
using ManagedStoreOpen = std::variant<std::unique_ptr<ManagedStore>, ManagedStoreError>;

// Experimental Windows fixture adapter, restricted to a provisioned direct child
// C:\SN021Managed-<12 lowercase hex>. No production install/adoption API.
// The caller runs as the trusted ordinary writer. Principal fields must come from
// an authenticated caller outside this adapter; no IPC authentication is implied.
class ManagedStore final {
public:
    ~ManagedStore();
    ManagedStore(const ManagedStore&) = delete;
    ManagedStore& operator=(const ManagedStore&) = delete;
    ManagedId run() const noexcept;
    std::string allowed_principal() const;
    ManagedStoreRead read();
    ManagedStoreSave save(const ManagedId& run, const ManagedCommitRequest& request);
    std::variant<ManagedReceiptLookup, ManagedStoreError> reconcile(
        const ManagedId& run, const ManagedCommitRequest& request);
private:
    struct Impl;
    explicit ManagedStore(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend ManagedStoreOpen open_managed_store(const std::string&);
};
ManagedStoreOpen open_managed_store(const std::string& root_leaf);

#ifdef SIMNODUS_MANAGED_STORE_TEST_HOOKS
// Test executable supplies barriers; false injects an ordinary failure. A killed
// process does not run cleanup. Production compilation has no callback or hook.
bool managed_store_test_boundary(const char* phase);
#endif
}
