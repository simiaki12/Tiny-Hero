#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <iostream>

struct PakEntry {
    char name[32];
    uint32_t offset;
    uint32_t size;
};

int main() {
    std::vector<std::string> files = {
        "assets/map1.bin",
        "assets/player.dat"
    };

    std::ofstream out("data.pak", std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open data.pak for writing\n";
        return 1;
    }

    uint16_t count = (uint16_t)files.size();
    out.write("PAK0", 4);
    out.write((char*)&count, 2);

    std::vector<PakEntry> entries(count);
    // Reserve space for the file table (filled in later)
    out.write((char*)entries.data(), count * sizeof(PakEntry));

    uint32_t offset = 4 + 2 + count * sizeof(PakEntry);

    for (int i = 0; i < count; i++) {
        std::ifstream in(files[i], std::ios::binary);
        if (!in) {
            std::cerr << "Failed to open: " << files[i] << "\n";
            return 1;
        }

        in.seekg(0, std::ios::end);
        uint32_t size = (uint32_t)in.tellg();
        in.seekg(0);

        strncpy(entries[i].name, files[i].c_str(), 31);
        entries[i].name[31] = '\0';
        entries[i].offset = offset;
        entries[i].size = size;

        std::vector<char> buffer(size);
        in.read(buffer.data(), size);

        out.seekp(offset);
        out.write(buffer.data(), size);

        offset += size;
    }

    // Write the completed file table
    out.seekp(6);
    out.write((char*)entries.data(), count * sizeof(PakEntry));

    std::cout << "Packed " << count << " file(s) into data.pak\n";
    return 0;
}
