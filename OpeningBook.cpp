#include "OpeningBook.h"
#include <cstdio>     // sscanf : parsing C tres simple
#include <fstream>
#include <iostream>

OpeningBook::OpeningBook() {}

// Lit un fichier ligne par ligne. Les lignes vides ou commencant par '#'
// sont ignorees. Le format attendu de chaque entree est :
//   <hash en hexa> <row> <col>
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

        // sscanf %llx -> unsigned long long, qu'on convertit ensuite en uint64_t.
        unsigned long long h;
        int row, col;
        if (std::sscanf(line.c_str(), "%llx %d %d", &h, &row, &col) == 3) {
            // Garde-fou : on ne charge que des coordonnees valides.
            if (row >= 0 && row < 9 && col >= 0 && col < 9) {
                entries[(uint64_t)h] = GameMove{row, col};
                ++loaded;
            }
        }
    }
    std::cout << "[Book] Charge " << loaded << " entrees depuis " << filename << std::endl;
}

// Ecrit l'integralite du livre dans le fichier (overwrite).
// On flush a la fin via le destructeur de std::ofstream.
void OpeningBook::save(const std::string& filename) const {
    std::ofstream f(filename);
    if (!f.is_open()) {
        std::cerr << "[Book] Impossible d'ecrire " << filename << std::endl;
        return;
    }

    // En-tete lisible (lignes ignorees au chargement).
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
