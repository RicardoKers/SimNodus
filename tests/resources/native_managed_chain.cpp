// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/managed_chain.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace simnodus::experimental;
unsigned checks = 0;
void check(bool condition, const char* label)
{
    ++checks;
    if(!condition) throw std::runtime_error(label);
}
ManagedId id(unsigned char value)
{
    ManagedId result{}; result.fill(value); return result;
}
std::shared_ptr<const ManagedChain> chain(const ManagedChainScope& scope,
    const std::vector<ManagedObservedRecord>& observations)
{
    const auto result = validate_managed_chain(scope, observations);
    const auto* valid = std::get_if<std::shared_ptr<const ManagedChain>>(&result);
    if(!valid) throw std::runtime_error("expected valid synthetic chain");
    return *valid;
}
void chain_error(const ManagedChainScope& scope, const std::vector<ManagedObservedRecord>& observations,
    ManagedChainError expected, const char* label)
{
    const auto result = validate_managed_chain(scope, observations);
    const auto* error = std::get_if<ManagedChainError>(&result);
    check(error && *error == expected, label);
}
void plan_error(const ManagedChain& current, const ManagedCommitRequest& request, ManagedId commit,
    ManagedChainError expected, const char* label)
{
    const auto result = prepare_managed_commit(current, request, commit);
    const auto* error = std::get_if<ManagedChainError>(&result);
    check(error && *error == expected, label);
}
ManagedRecord decoded(const ManagedObservedRecord& observation)
{
    return std::get<DecodedManagedRecord>(decode_managed_record(observation.bytes)).record;
}
void replace(ManagedObservedRecord& observation, const ManagedRecord& record)
{
    observation.bytes = std::get<std::string>(encode_managed_record(record));
}
}
int main(int argc, char** argv)
{
    using namespace simnodus::experimental;
    try {
        if(argc != 2) return 2;
        std::ifstream input(argv[1], std::ios::binary);
        if(!input) return 2;
        const std::string project(std::istreambuf_iterator<char>{input}, {});
        const ManagedChainScope scope{id(1), id(2), 7};
        std::vector<ManagedObservedRecord> observations;
        auto current = chain(scope, observations);
        check(current->current() == ManagedToken{} && current->entries().empty(), "fresh chain");
        ManagedCommitRequest request{scope.generation, scope.document, id(3),
            std::string("\1\1\0\0\0\0\0\5\25\0\0\0", 12), {},
            {id(4), {9, id(5)}, 1, "C:/missing-inert-root"}, project};
        const auto original_request = request;
        check(std::holds_alternative<ManagedReceiptNotObserved>(lookup_managed_receipt(*current, request)),
            "absence is only not observed");
        auto initial = prepare_managed_commit(*current, request, id(6));
        auto* prepared = std::get_if<ManagedPreparedCommit>(&initial);
        check(prepared && prepared->leaf == "00000001.commit" && prepared->revision == 1, "import plan");
        const auto parsed = std::get<DecodedManagedRecord>(decode_managed_record(prepared->bytes));
        check(prepared->record_digest == parsed.record_digest && prepared->request_digest == parsed.request_digest,
            "plan digest bindings");
        observations.push_back({prepared->leaf, prepared->bytes, {scope.volume, id(11)}});
        current = chain(scope, observations);
        const auto first = current->current();
        check(first.identity == observations[0].identity && first.digest == prepared->record_digest,
            "observed identity in token");
        observations[0].bytes[0] = 'X';
        check(current->entries()[0].decoded.record.project == project, "owned snapshot after mutation");
        observations[0].bytes = prepared->bytes;
        const auto retry = prepare_managed_commit(*current, request, {});
        check(std::holds_alternative<ManagedReceipt>(retry)
            && std::get<ManagedReceipt>(retry).token == first, "retry before stale and commit allocation");
        auto wrong = request; wrong.principal_sid.back() = 1;
        plan_error(*current, wrong, id(8), ManagedChainError::operation_conflict, "wrong principal no receipt");
        wrong = request; wrong.project += ' ';
        plan_error(*current, wrong, id(8), ManagedChainError::operation_conflict, "same operation different bytes");
        wrong = request; wrong.context.locator = "D:/other";
        plan_error(*current, wrong, id(8), ManagedChainError::operation_conflict, "same operation different context");
        wrong = request; wrong.expected = first;
        plan_error(*current, wrong, id(8), ManagedChainError::operation_conflict, "same operation different expected");
        wrong = request; wrong.generation = id(99);
        plan_error(*current, wrong, id(8), ManagedChainError::scope, "wrong generation");
        wrong = request; wrong.document = id(99);
        plan_error(*current, wrong, id(8), ManagedChainError::scope, "wrong document");
        wrong = request; wrong.operation = {};
        plan_error(*current, wrong, id(8), ManagedChainError::request, "null operation");
        wrong = request; wrong.operation = id(7);
        plan_error(*current, wrong, id(8), ManagedChainError::stale, "second import stale");
        request.operation = id(7); request.expected = first;
        wrong = request; wrong.expected.identity.file = id(99);
        plan_error(*current, wrong, id(8), ManagedChainError::stale, "expected identity changed");
        wrong = request; wrong.expected.digest.fill(99);
        plan_error(*current, wrong, id(8), ManagedChainError::stale, "expected digest changed");
        plan_error(*current, request, id(6), ManagedChainError::commit_id, "commit ID reuse");
        plan_error(*current, request, {}, ManagedChainError::commit_id, "null commit ID");
        wrong = request; wrong.project = "{}";
        plan_error(*current, wrong, id(8), ManagedChainError::request, "invalid project before staging");
        wrong = request; wrong.context.policy = 2;
        plan_error(*current, wrong, id(8), ManagedChainError::request, "invalid root policy");
        wrong = request; wrong.principal_sid[0] = 2;
        plan_error(*current, wrong, id(8), ManagedChainError::request, "invalid SID");
        request.project += ' '; // B differs in exact bytes, still a valid project.
        const auto second_request = request;
        const auto second = std::get<ManagedPreparedCommit>(prepare_managed_commit(*current, request, id(8)));
        auto competitor = request; competitor.operation = id(9);
        check(std::holds_alternative<ManagedPreparedCommit>(prepare_managed_commit(*current, competitor, id(10))),
            "plans are not publication authority");
        observations.push_back({second.leaf, second.bytes, {scope.volume, id(12)}});
        current = chain(scope, observations);
        plan_error(*current, competitor, id(10), ManagedChainError::stale, "competitor rechecked against newer snapshot");
        request.operation = id(9); request.expected = current->current(); request.project = project;
        const auto third = std::get<ManagedPreparedCommit>(prepare_managed_commit(*current, request, id(10)));
        observations.push_back({third.leaf, third.bytes, {scope.volume, id(13)}});
        current = chain(scope, observations);
        check(current->entries().back().decoded.record.project == current->entries().front().decoded.record.project
            && current->current() != first, "A-B-A has distinct token");
        wrong = request; wrong.operation = id(15); wrong.expected = first;
        plan_error(*current, wrong, id(16), ManagedChainError::stale, "ABA cannot revive old token");
        auto shuffled = observations; std::reverse(shuffled.begin(), shuffled.end());
        check(chain(scope, shuffled)->current() == current->current(), "enumeration order irrelevant");
        const auto old_receipt = lookup_managed_receipt(*current, original_request);
        check(std::holds_alternative<ManagedReceipt>(old_receipt)
            && std::get<ManagedReceipt>(old_receipt).token == first, "lost reply resolved after later revisions");
        check(std::holds_alternative<ManagedReceipt>(lookup_managed_receipt(*current, second_request)),
            "middle receipt retained");
        const auto before = observations;
        auto bad = observations; bad.erase(bad.begin() + 1);
        chain_error(scope, bad, ManagedChainError::predecessor, "gap refuses recovery");
        bad = observations; bad[1] = bad[0];
        chain_error(scope, bad, ManagedChainError::predecessor, "duplicate revision refuses recovery");
        bad = observations; bad[1].identity = bad[0].identity;
        chain_error(scope, bad, ManagedChainError::identity, "hardlink identity alias");
        bad = observations; bad[0].identity.file = id(99);
        chain_error(scope, bad, ManagedChainError::predecessor, "replaced predecessor object");
        bad = observations; bad[0].identity.volume++;
        chain_error(scope, bad, ManagedChainError::identity, "wrong volume");
        bad = observations; bad[0].identity.file = {};
        chain_error(scope, bad, ManagedChainError::identity, "null observed file ID");
        bad = observations; bad[0].bytes.back() ^= 1;
        chain_error(scope, bad, ManagedChainError::record, "corrupt final record");
        bad = observations; auto altered = decoded(bad[2]); altered.operation = original_request.operation;
        replace(bad[2], altered);
        chain_error(scope, bad, ManagedChainError::duplicate_operation, "fully rehashed duplicate operation");
        bad = observations; altered = decoded(bad[2]); altered.commit = id(6); replace(bad[2], altered);
        chain_error(scope, bad, ManagedChainError::duplicate_commit, "nonadjacent commit reuse");
        bad = observations; altered = decoded(bad[1]); altered.predecessor.digest.fill(99); replace(bad[1], altered);
        chain_error(scope, bad, ManagedChainError::predecessor, "rehashed false predecessor digest");
        for(const auto name : {"00000001.COMMIT", "00000001.commit ", "../00001.commit", "00000002.commit",
                              "stage-01234567890123456789012345678901.tmp"}) {
            bad = observations; bad[0].leaf = name;
            chain_error(scope, bad, ManagedChainError::name, "noncanonical or misplaced name");
        }
        auto foreign_scope = scope; foreign_scope.generation = id(99);
        chain_error(foreign_scope, observations, ManagedChainError::scope, "foreign chain scope");
        bad = observations; bad[0].bytes.resize(managed_record_max_bytes + 1);
        chain_error(scope, bad, ManagedChainError::budget, "record budget before decode");
        check(observations[0].bytes == before[0].bytes && observations[2].bytes == before[2].bytes,
            "refusals preserve caller observations");
        // All 64 receipts remain available. No pruning is used to admit revision 65.
        while(observations.size() < managed_revision_limit) {
            request.operation = id(static_cast<unsigned char>(20 + observations.size()));
            request.expected = current->current();
            const auto commit = id(static_cast<unsigned char>(90 + observations.size()));
            const auto next = std::get<ManagedPreparedCommit>(prepare_managed_commit(*current, request, commit));
            observations.push_back({next.leaf, next.bytes, {scope.volume, id(static_cast<unsigned char>(160 + observations.size()))}});
            current = chain(scope, observations);
        }
        check(current->current().revision == 64 && observations.back().leaf == "00000040.commit", "capacity boundary");
        request.expected = current->current(); request.operation = id(240);
        plan_error(*current, request, id(241), ManagedChainError::capacity, "capacity before staging");
        check(std::holds_alternative<ManagedReceipt>(prepare_managed_commit(*current, original_request, {})),
            "old receipt accessible at capacity");
        check(std::holds_alternative<ManagedReceiptNotObserved>(lookup_managed_receipt(*current, request)),
            "absence at capacity is still not observed");
        bad = observations; bad.push_back(observations.back());
        chain_error(scope, bad, ManagedChainError::budget, "65 records rejected before parse");
        // A valid prefix alone cannot prove that physical enumeration was complete.
        bad = {observations.front()};
        check(chain(scope, bad)->current() == first, "prefix acceptance requires external completeness gate");
        std::cout << checks << " bounded chain, receipt, conflict and preservation checks passed; synthetic observations only\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
