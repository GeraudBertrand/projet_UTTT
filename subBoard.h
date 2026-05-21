#ifndef SUBBOARD_H
#define SUBBOARD_H

#include "main.h" // Pour Winner

// =============================================================================
//  SubBoard : une sous-grille 3x3 (= une mini partie de tic-tac-toe).
//
//  Le plateau UTTT est un 3x3 de SubBoard. C'est ici qu'on suit qui a joue
//  quelle case et qui a gagne la sous-grille. Toutes les operations sont
//  O(1) ou O(constante).
//
//  Les tables statiques `LINES` et `LINE_PAIRS` decrivent la geometrie des
//  lignes du tic-tac-toe ; elles sont reutilisees par UltimateBoard (le
//  plateau global est lui aussi une grille 3x3).
// =============================================================================
class SubBoard {
private:
    Winner cells[3][3];       // contenu de chaque case (NO_WINNER si vide)
    Winner boardWinner;       // gagnant de cette sous-grille (NO_WINNER si non decidee)
    int    filledCellsCount;  // nb de cases occupees, pour detecter le "plein"

public:
    SubBoard();

    // -------- Mutations --------
    void playMove(int r, int c, Winner player); // pose un pion et met a jour boardWinner
    void undoMove(int r, int c);                // defait un coup (utilise par le minimax)

    // -------- Accesseurs --------
    Winner getWinner() const   { return boardWinner; }
    bool   isFull() const      { return filledCellsCount >= 9; }
    bool   isFinished() const  { return boardWinner != NO_WINNER || isFull(); }
    Winner getCell(int r, int c) const { return cells[r][c]; }

    // -------- Helpers tic-tac-toe --------
    // Ces methodes operent uniquement sur l'etat 3x3, donc elles vivent ici
    // (et pas dans MinimaxAI qui en a besoin pour son evaluation).

    // Vrai si poser 'player' en (r, c) complete une ligne (rangee/colonne/diag).
    bool wouldCompleteLine(int r, int c, Winner player) const;

    // Vrai si 'player' a un coup gagnant immediat dans la sous-grille.
    bool hasOneMoveWin(Winner player) const;

    // -------- Tables de lignes 3x3 precalculees (utilisees aussi par UltimateBoard) --------

    // Les 8 lignes du tic-tac-toe : 3 rangees + 3 colonnes + 2 diagonales.
    // Chaque ligne contient 3 cases sous la forme (r, c).
    static const int LINES[8][3][2];

    // Pour la case (r, c), les coordonnees des 2 AUTRES cases de chaque ligne
    // qui passe par elle (format {r1, c1, r2, c2}).
    // r1 == -1 signale la fin (une case appartient a 2 a 4 lignes).
    static const int LINE_PAIRS[3][3][4][4];

private:
    void checkWinCondition(int r, int c, Winner player);
};

#endif
