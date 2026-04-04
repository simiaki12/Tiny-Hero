#include "pak.h"
#include <fstream>
#include <cstring>

static std::vector<PakEntry> pakEntries;
static std::ifstream pakFile;

bool pakOpen(const char* filename) {
    pakFile.open(filename, std::ios::binary);
    if (!pakFile) return false;

    char magic[4];
    pakFile.read(magic, 4);
    if (strncmp(magic, "PAK0", 4) != 0) return false;

    uint16_t fileCount;
    pakFile.read((char*)&fileCount, 2);

    pakEntries.resize(fileCount);
    pakFile.read((char*)pakEntries.data(), fileCount * sizeof(PakEntry));

    return true;
}

const PakEntry* pakFind(const std::string& name) {
    for (const auto& entry : pakEntries) {
        if (name == entry.name)
            return &entry;
    }
    return nullptr;
}

std::vector<uint8_t> pakRead(const std::string& name) {
    std::vector<uint8_t> data;
    const PakEntry* entry = pakFind(name);
    if (!entry) return data;

    data.resize(entry->size);
    pakFile.seekg(entry->offset);
    pakFile.read((char*)data.data(), entry->size);

    return data;
}

void pakClose() {
    pakFile.close();
    pakEntries.clear();
}
