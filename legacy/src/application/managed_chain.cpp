// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/managed_chain.hpp"
#include "application/declaration_ingress.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace simnodus::experimental {
namespace {
using Error = ManagedChainError;
template<class T> bool zero(const T& value)
{
    return std::all_of(value.begin(), value.end(), [](auto b) { return b == 0; });
}
void require(bool condition, Error error)
{
    if(!condition) throw error;
}
std::string leaf(std::uint32_t revision)
{
    std::string name(8, '0');
    for(unsigned i = 0; i < 8; ++i) name[7 - i] = "0123456789abcdef"[(revision >> (4 * i)) & 15];
    return name + ".commit";
}
ManagedToken observed_token(const DecodedManagedRecord& decoded, const ManagedIdentity& identity)
{
    const auto& record = decoded.record;
    return {record.generation, record.document, record.revision, record.commit, decoded.record_digest, identity};
}
void check_request_scope(const ManagedChain& chain, const ManagedCommitRequest& request)
{
    require(request.generation == chain.scope().generation && request.document == chain.scope().document,
        Error::scope);
    require(!zero(request.operation) && request.principal_sid.size() >= 12
        && request.principal_sid.size() <= 68 && !request.project.empty()
        && request.project.size() <= declaration_max_bytes && request.context.locator.size() <= 4096,
        Error::request);
}
bool same_request(const ManagedRecord& record, const ManagedCommitRequest& request)
{
    // Exact bytes, not merely digest equality. Scope and operation already match.
    return record.principal_sid == request.principal_sid && record.predecessor == request.expected
        && record.context == request.context && record.project == request.project;
}
}
ManagedToken ManagedChain::current() const noexcept
{
    return entries_.empty() ? ManagedToken{} : entries_.back().token;
}
ManagedChainResult validate_managed_chain(const ManagedChainScope& scope,
    std::span<const ManagedObservedRecord> observations)
{
    try {
        require(!zero(scope.generation) && !zero(scope.document), Error::scope);
        require(observations.size() <= managed_revision_limit, Error::budget);
        constexpr std::size_t byte_limit = 128 * 1024 * 1024;
        std::size_t total = 0;
        for(const auto& observation : observations) {
            require(observation.bytes.size() <= managed_record_max_bytes
                && observation.bytes.size() <= byte_limit - total, Error::budget);
            total += observation.bytes.size();
            require(observation.leaf.size() == 15, Error::name);
        }
        auto chain = std::shared_ptr<ManagedChain>(new ManagedChain);
        chain->scope_ = scope;
        chain->entries_.reserve(observations.size());
        for(const auto& observation : observations) {
            require(!zero(observation.identity.file) && observation.identity.volume == scope.volume, Error::identity);
            auto decoded = decode_managed_record(observation.bytes);
            if(const auto* error = std::get_if<ManagedRecordError>(&decoded))
                require(*error != ManagedRecordError::memory, Error::memory);
            auto* value = std::get_if<DecodedManagedRecord>(&decoded);
            require(value != nullptr, Error::record);
            const auto& record = value->record;
            require(record.generation == scope.generation && record.document == scope.document, Error::scope);
            require(observation.leaf == leaf(record.revision), Error::name);
            const auto token = observed_token(*value, observation.identity);
            chain->entries_.push_back({std::move(*value), token});
        }
        auto& entries = chain->entries_;
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.token.revision < b.token.revision;
        });
        ManagedToken previous;
        for(std::size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            require(entry.token.revision == i + 1 && entry.decoded.record.predecessor == previous, Error::predecessor);
            for(std::size_t j = 0; j < i; ++j) {
                require(entry.token.identity != entries[j].token.identity, Error::identity);
                require(entry.token.commit != entries[j].token.commit, Error::duplicate_commit);
                require(entry.decoded.record.operation != entries[j].decoded.record.operation, Error::duplicate_operation);
            }
            previous = entry.token;
        }
        return std::shared_ptr<const ManagedChain>(std::move(chain));
    } catch(Error error) { return error; }
    catch(const std::bad_alloc&) { return Error::memory; }
    catch(const std::length_error&) { return Error::memory; }
}
ManagedReceiptLookup lookup_managed_receipt(const ManagedChain& chain, const ManagedCommitRequest& request)
{
    try {
        check_request_scope(chain, request);
        for(const auto& entry : chain.entries()) {
            if(entry.decoded.record.operation != request.operation) continue;
            require(same_request(entry.decoded.record, request), Error::operation_conflict);
            return ManagedReceipt{request.operation, entry.token, entry.decoded.request_digest};
        }
        return ManagedReceiptNotObserved{};
    } catch(Error error) { return error; }
}
ManagedCommitPlan prepare_managed_commit(const ManagedChain& chain,
    const ManagedCommitRequest& request, const ManagedId& commit)
{
    try {
        const auto lookup = lookup_managed_receipt(chain, request);
        if(const auto* receipt = std::get_if<ManagedReceipt>(&lookup)) return *receipt;
        if(const auto* error = std::get_if<Error>(&lookup)) return *error;
        require(request.expected == chain.current(), Error::stale);
        require(chain.entries().size() < managed_revision_limit, Error::capacity);
        require(!zero(commit), Error::commit_id);
        for(const auto& entry : chain.entries()) require(entry.token.commit != commit, Error::commit_id);
        const auto revision = static_cast<std::uint32_t>(chain.entries().size() + 1);
        ManagedRecord record{request.generation, request.document, commit, request.operation, revision,
            request.expected, request.principal_sid, request.context, request.project};
        auto encoded = encode_managed_record(record);
        auto* bytes = std::get_if<std::string>(&encoded);
        if(!bytes) {
            if(std::get<ManagedRecordError>(encoded) == ManagedRecordError::memory) return Error::memory;
            return Error::request;
        }
        ManagedPreparedCommit prepared{leaf(revision), std::move(*bytes), revision, commit, request.operation, {}, {}};
        // These fixed offsets belong to the already validated canonical codec.
        std::copy_n(prepared.bytes.begin() + 204, 32, prepared.request_digest.begin());
        std::copy_n(prepared.bytes.end() - 32, 32, prepared.record_digest.begin());
        return prepared;
    } catch(Error error) { return error; }
    catch(const std::bad_alloc&) { return Error::memory; }
    catch(const std::length_error&) { return Error::memory; }
}
}
