#include "OpeningBook.h"
#include <fstream>
#include <iostream>
#include <sstream>

OpeningBook::OpeningBook() {}

void OpeningBook::load(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cout << "[Book] Aucun livre existant a " << filename
                  << " (sera cree a la sauvegarde)" << std::endl;
        return;
    }

    std::string line;
    int loaded = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        uint64_t hash = 0;
        int row = -1, col = -1;
        // Format : <hash en hex> <row> <col>
        if (iss >> std::hex >> hash >> std::dec >> row >> col) {
            if (row >= 0 && row < 9 && col >= 0 && col < 9) {
                entries[hash] = GameMove{row, col};
                ++loaded;
            }
        }
    }
    std::cout << "[Book] Charge " << loaded << " entrees depuis " << filename << std::endl;
}

void OpeningBook::save(const std::string& filename) const {
    std::ofstream f(filename);
    if (!f.is_open()) {
        std::cerr << "[Book] Impossible d'ecrire " << filename << std::endl;
        return;
    }
    f << "# OpeningBook : <hash hex> <row> <col>" << std::endl;
    f << "# " << entries.size() << " entrees" << std::endl;
    for (const auto& kv : entries) {
        f << std::hex << kv.first << std::dec << " "
          << kv.second.row << " " << kv.second.col << "\n";
    }
    std::cout << "[Book] Sauve " << entries.size() << " entrees vers " << filename << std::endl;
}

bool OpeningBook::lookup(uint64_t hash, GameMove& out) const {
    auto it = entries.find(hash);
    if (it == entries.end()) return false;
    out = it->second;
    return true;
}

void OpeningBook::insert(uint64_t hash, GameMove move) {
    entries[hash] = move;
}
