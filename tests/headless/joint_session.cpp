// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "core/joint_session.hpp"
#include <iostream>
#include <limits>
int main()
{
    using namespace simnodus;
    int failures = 0;
    auto check = [&](bool value) { if(!value) ++failures; };
    auto ready = [] {
        JointSession s(true);
        s.begin(5'000'000, false);
        s.observe(2'010'875);
        return s;
    };
    const Nanoseconds sink = 2'010'875;
    const CpuOutcome valid{0, sink, 5'000'000, 2'989'125, true, {&sink, 1}};
    auto s = ready();
    check(s.snapshot().committed_ns == 0 && s.snapshot().cpu_acknowledged_ns == 0);
    check(s.acknowledge(valid) == SessionError::none);
    check(s.snapshot().committed_ns == 0 && s.snapshot().acknowledgements == 1);
    check(s.analog({sink, sink*1e-9, 0}) == SessionError::none);
    check(s.snapshot().committed_ns == 0);
    check(s.commit() == SessionError::none && s.snapshot().committed_ns == sink);
    check(s.snapshot().discarded_ns == valid.unused);
    check(s.begin(5'000'000, false) == SessionError::none);
    check(s.observe(sink-1) == SessionError::observation);
    check(s.snapshot().committed_ns == sink && s.begin(5'000'000, false) != SessionError::none);
    for(int mutation=0; mutation<8; ++mutation) {
        auto bad = valid;
        if(mutation==0) bad.start=1;
        if(mutation==1) bad.requested=100'000'000;
        if(mutation==2) bad.unused=std::numeric_limits<Nanoseconds>::max();
        if(mutation==3) bad.end=std::numeric_limits<Nanoseconds>::max();
        if(mutation==4) bad.cancelled=false;
        if(mutation==5) bad.sinks={};
        if(mutation==6) bad.unused=0;
        if(mutation==7) bad.end=sink-1;
        auto t=ready();
        check(t.acknowledge(bad)==SessionError::accounting);
        check(t.snapshot().acknowledgements==0 && t.snapshot().commits==0);
    }
    auto premature=ready();
    check(premature.commit()==SessionError::order);
    auto duplicate=ready(); duplicate.acknowledge(valid);
    check(duplicate.acknowledge(valid)==SessionError::order);
    auto failed=ready(); failed.acknowledge(valid); failed.abort();
    check(failed.snapshot().acknowledgements==1 && failed.snapshot().commits==0);
    check(failed.analog({sink, sink*1e-9, 0})!=SessionError::none);
    auto mismatch=ready(); mismatch.acknowledge(valid);
    check(mismatch.analog({sink, sink*1e-9+2e-12, 0})==SessionError::analog);
    JointSession unsupported;
    check(unsupported.begin(5'000'000, false)==SessionError::capability);
    JointSession unpaced(true);
    check(unpaced.begin(100'000'000, false)==SessionError::capability);
    JointSession fresh(true);
    check(fresh.snapshot().commits==0 && fresh.snapshot().committed_ns==0);
    check(fresh.begin(100'000'000, true)==SessionError::none);
    check(fresh.begin(5'000'000, false)==SessionError::order);
    std::cout << failures << " joint session failures\n";
    return failures ? 1 : 0;
}
