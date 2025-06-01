#pragma once
#include <set>
#include <string>
#include <cstdio>

static std::set<std::string> __cov_hits;

extern "C" void __cov_hit(const char* id) {
    __cov_hits.insert(id);
}

extern "C" void __cov_dump() {
    FILE* f = fopen("coverage.txt", "w");
    for (const auto& s : __cov_hits)
        fprintf(f, "HIT: %s\n", s.c_str());
    fclose(f);
}
