#ifndef OPENINGBOOK_H
#define OPENINGBOOK_H

#include "main.h"
#include <cstdint>
#include <string>
#include <unordered_map>

// =============================================================================
//  OpeningBook : table persistante "position -> meilleur coup".
//
//  La cle est le hash Zobrist d'UltimateBoard. La valeur est le coup que l'IA
//  a juge meilleur lors d'une recherche profonde precedente (calcule la
//  premiere fois qu'on rencontre la position, puis reutilise).
//
//  Format de fichier (texte, lisible) : une entree par ligne
//      <hash en hexa> <row> <col>
//  Les lignes commencant par '#' sont des commentaires.
// =============================================================================
class OpeningBook {
public:
    OpeningBook();

    // Charge le livre depuis un fichier. Fichier absent = livre vide (OK).
    void load(const std::string& filename);

    // Sauvegarde le livre vers un fichier.
    void save(const std::string& filename) const;

    // Cherche un coup pour la position (par son hash Zobrist).
    // Renvoie true et remplit `out` si trouve.
    bool lookup(uint64_t hash, GameMove& out) const;

    // Ajoute ou remplace une entree.
    void insert(uint64_t hash, GameMove move);

    int  size() const  { return (int)entries.size(); }
    bool empty() const { return entries.empty(); }

private:
    std::unordered_map<uint64_t, GameMove> entries;
};

#endif
