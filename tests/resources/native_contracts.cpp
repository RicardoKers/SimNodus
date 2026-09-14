// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/local_resources.hpp"
#include <iostream>
#include <type_traits>

using namespace simnodus;
static_assert(std::is_const_v<std::remove_reference_t<decltype((*ResourceSnapshots{})[0])>>);
int main()
{
    unsigned cases = 0;
    auto rejected = [&](const std::vector<ResourceRequest>& input, ResourceErrorCode code,
                        const std::string& root = "Q:/missing-root") {
        ++cases;
        auto result = verify_local_resources(root, input);
        const auto* error = std::get_if<ResourceError>(&result);
        if(!error || error->code != code) {
            std::cerr << "Unexpected outcome in case " << cases << '\n';
            return false;
        }
        return true;
    };
    const ResourceRequest valid{"owned", "notice", "LICENSE", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
    if(!rejected({}, ResourceErrorCode::budget)) return 1;
    if(!rejected(std::vector<ResourceRequest>(257, valid), ResourceErrorCode::budget)) return 1;
    for(const std::string p : {"../x", "a/../x", "C:/x", "C:x", "//host/x", "a\\b", "a:b",
        "NUL.txt", "COM1", "x.", "x ", "x~1", "a//b", "a/", ".hidden", "a/%2e", "a?b"}) {
        auto r = valid; r.path = p;
        if(!rejected({r}, ResourceErrorCode::path)) return 1;
    }
    for(const std::string bad : std::vector<std::string>{"", "Owned", "a.b", std::string(65, 'a')}) {
        auto r = valid; r.resource = bad;
        if(!rejected({r}, ResourceErrorCode::id)) return 1;
    }
    auto r = valid; r.bytes = resource_max_file_bytes + 1;
    if(!rejected({r}, ResourceErrorCode::budget)) return 1;
    r.bytes = UINT64_MAX;
    if(!rejected({r}, ResourceErrorCode::budget)) return 1;
    r = valid; r.sha256[0] = 'A';
    if(!rejected({r}, ResourceErrorCode::hash)) return 1;
    r = valid; r.resource = "other"; r.path = "license";
    if(!rejected({valid, r}, ResourceErrorCode::path)) return 1;
    r.path = "LICENSE/child";
    if(!rejected({valid, r}, ResourceErrorCode::path)) return 1;
    r.path = "other"; r.resource = "notice";
    if(!rejected({valid, r}, ResourceErrorCode::id)) return 1;
    std::vector<ResourceRequest> total;
    for(unsigned i = 0; i < 5; ++i) {
        r = valid; r.resource += std::to_string(i); r.path += std::to_string(i);
        r.bytes = resource_max_file_bytes; total.push_back(r);
    }
    if(!rejected(total, ResourceErrorCode::budget)) return 1;
    total.clear();
    for(unsigned i = 0; i < 33; ++i) {
        r = valid; r.dependency += std::to_string(i); r.path += std::to_string(i); total.push_back(r);
    }
    if(!rejected(total, ResourceErrorCode::budget)) return 1;
    for(const std::string root : {".", "C:foo", "//host/share", "C:/a/../b", "C:/a~1", "C:/NUL",
                                 "C:/a//b", "C:/a:b", "C:/end.", "C:/end "})
        if(!rejected({valid}, ResourceErrorCode::root, root)) return 1;
#ifndef _WIN32
    if(!rejected({valid}, ResourceErrorCode::platform)) return 1;
#endif
    std::cout << cases << " native typed-input cases passed\n";
}
