// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/managed_record.hpp"
#include <memory>
#include <span>
#include <vector>

namespace simnodus::experimental {
struct ManagedChainScope {
    ManagedId generation{}, document{};
    std::uint64_t volume{};
};
// Observations supplied by a future handle-retaining filesystem adapter. These
// structs cannot prove identity, security, namespace completeness or writer locking.
struct ManagedObservedRecord {
    std::string leaf, bytes;
    ManagedIdentity identity;
};
struct ManagedChainEntry {
    DecodedManagedRecord decoded;
    ManagedToken token;
};
class ManagedChain;
enum class ManagedChainError {
    scope, budget, record, name, identity, predecessor, duplicate_commit,
    duplicate_operation, request, operation_conflict, stale, capacity, commit_id, memory
};
using ManagedChainResult = std::variant<std::shared_ptr<const ManagedChain>, ManagedChainError>;
ManagedChainResult validate_managed_chain(const ManagedChainScope& scope,
    std::span<const ManagedObservedRecord> observations);

class ManagedChain final {
public:
    const ManagedChainScope& scope() const noexcept { return scope_; }
    const std::vector<ManagedChainEntry>& entries() const noexcept { return entries_; }
    ManagedToken current() const noexcept;
private:
    ManagedChain() = default;
    ManagedChainScope scope_;
    std::vector<ManagedChainEntry> entries_;
    friend ManagedChainResult validate_managed_chain(const ManagedChainScope&,
        std::span<const ManagedObservedRecord>);
};

struct ManagedCommitRequest {
    ManagedId generation{}, document{}, operation{};
    // Must come from authenticated/authorized operation input, never project data.
    std::string principal_sid;
    ManagedToken expected;
    ManagedContext context;
    std::string project;
};
struct ManagedReceipt {
    ManagedId operation{};
    ManagedToken token;
    ManagedDigest request_digest{};
};
// Only absence in this supplied snapshot. NOT a definite not-committed outcome.
struct ManagedReceiptNotObserved {};
using ManagedReceiptLookup = std::variant<ManagedReceipt, ManagedReceiptNotObserved, ManagedChainError>;
ManagedReceiptLookup lookup_managed_receipt(const ManagedChain& chain, const ManagedCommitRequest& request);

struct ManagedPreparedCommit {
    std::string leaf, bytes;
    std::uint32_t revision{};
    ManagedId commit{}, operation{};
    ManagedDigest request_digest{}, record_digest{};
};
using ManagedCommitPlan = std::variant<ManagedPreparedCommit, ManagedReceipt, ManagedChainError>;
// Receipt lookup precedes stale/capacity checks. For a fresh operation, the caller
// supplies a fresh commit ID. Preparation allocates owned inert bytes only.
// The caller must independently authenticate, fence runs, hold writer ownership,
// validate a complete physical snapshot, and recheck it before publication.
ManagedCommitPlan prepare_managed_commit(const ManagedChain& chain,
    const ManagedCommitRequest& request, const ManagedId& commit);
}
