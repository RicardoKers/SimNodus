// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/renode/cancellation_result.hpp"
#include <iostream>
#include <string>
#include <vector>
int main()
{
    using namespace simnodus;
    using namespace simnodus::renode;
    int failures = 0;
    auto check = [&](bool value) { if(!value) ++failures; };
    const std::string valid = "start=0\nrequested=5000000\nend=2010875\nreason=cancelled\nunused=2989125\nsinks=2010875\ncancelled=True";
    CancellationResult result;
    check(parse_cancellation(valid, result) == ResultStatus::ready);
    check(result.start == 0 && result.end == 2010875 && result.unused == 2989125);
    JointSession session(true);
    session.begin(5000000, false);
    session.observe(2010875);
    check(session.acknowledge(result.outcome()) == SessionError::none);
    check(session.snapshot().commits == 0);
    auto crlf = valid;
    for(std::size_t i=0; (i=crlf.find('\n',i)) != std::string::npos; i+=2) crlf.insert(i,"\r");
    check(parse_cancellation(crlf+"\r\n", result) == ResultStatus::ready);
    std::vector<std::string> invalid{"", valid+"\nstart=0", valid+"\nunknown=1", valid+"\n\n",
        valid.substr(0,valid.find("cancelled=")), std::string(4097,'a')};
    for(const auto& replacement : {"-1", "+1", "18446744073709551616", "0.0", "0junk", " 0", ""}) {
        auto text=valid; text.replace(text.find("start=0"),7,std::string("start=")+replacement);
        invalid.push_back(text);
    }
    for(const auto& field : {std::pair{"sinks=2010875", "sinks=2010875,2010875"},
                             std::pair{"reason=cancelled", "reason=completed"},
                             std::pair{"cancelled=True", "cancelled=False"}}) {
        auto text=valid; text.replace(text.find(field.first),std::string(field.first).size(),field.second);
        invalid.push_back(text);
    }
    for(const auto& text : invalid) check(parse_cancellation(text,result)==ResultStatus::malformed);
    const auto start = ResultDeadline::Clock::time_point{};
    const ResultDeadline deadline(start,std::chrono::milliseconds(2000));
    check(!deadline.expired(start+std::chrono::milliseconds(1999)));
    check(deadline.expired(start+std::chrono::milliseconds(2000)));
    check(deadline.expired(start+std::chrono::milliseconds(2001)));
    // Repeated observations of a missing/denied file do not restart the budget.
    for(int ms=0;ms<2000;++ms) check(!deadline.expired(start+std::chrono::milliseconds(ms)));
    check(deadline.expired(start+std::chrono::milliseconds(2000)));
    std::cout << failures << " cancellation result failures\n";
    return failures ? 1 : 0;
}
