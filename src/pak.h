#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct PakEntry {
    char name[32];
    uint32_t offset;
    uint32_t size;
};

bool pakOpen(const char* filename);
const PakEntry* pakFind(const std::string& name);
std::vector<uint8_t> pakRead(const std::string& name);
void pakClose();
