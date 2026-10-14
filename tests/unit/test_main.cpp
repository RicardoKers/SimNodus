// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

int main(int argc, char** argv)
{
    return simnodus::test::run_all(argc > 1 ? argv[1] : "");
}
