#ifndef OPENINGBOOK_H
#define OPENINGBOOK_H

#include "main.h"
#include <cstdint>
#include <string>
#include <unordered_map>

class OpeningBook {
public:
    OpeningBook();

    // Charge un livre depuis un fichier texte. Format : "<hash_hex> <row> <col>" par ligne.
    // Si le fichier n'existe pas, c'est OK (livre vide).
    void load(const std::string& filename);

    // Sauvegarde le livre vers un fichier texte.
    void save(const std::string& filename) const;

    // Cherche un coup pour la position (par hash Zobrist).
    bool lookup(uint64_t hash, GameMove& out) const;

    // Ajoute / remplace une entree.
    void insert(uint64_t hash, GameMove move);

    int size() const { return (int)entries.size(); }
    bool empty() const { return entries.empty(); }

private:
    std::unordered_map<uint64_t, GameMove> entries;
};

#endif
