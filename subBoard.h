#ifndef SUBBOARD_H
#define SUBBOARD_H

#include "main.h" // Pour Winner

class SubBoard {
private:
    Winner cells[3][3];
    Winner boardWinner;
    int filledCellsCount;

public:
    SubBoard();

    // Jouer un coup (localRow/Col entre 0 et 2)
    void playMove(int r, int c, Winner player);
    void undoMove(int r, int c);

    // Accesseurs
    Winner getWinner() const { return boardWinner; }
    bool isFull() const { return filledCellsCount >= 9; }
    bool isFinished() const { return boardWinner != NO_WINNER || isFull(); }
    Winner getCell(int r, int c) const { return cells[r][c]; }

    // Helpers tic-tac-toe operant sur cette sous-grille.
    // (Ces methodes etaient dans MinimaxAI ; elles appartiennent en fait a SubBoard
    // puisqu'elles ne dependent que de l'etat de la sous-grille.)
    bool wouldCompleteLine(int r, int c, Winner player) const;
    bool hasOneMoveWin(Winner player) const;

    // -------- Tables de lignes 3x3 precalculees --------
    // Comme le plateau global est aussi une grille 3x3 de sous-grilles,
    // UltimateBoard reutilise ces tables.

    // Les 8 lignes : 3 rangees + 3 colonnes + 2 diagonales.
    // Chaque ligne contient 3 cases (r, c).
    static const int LINES[8][3][2];

    // Pour la case (r, c), les coordonnees des 2 AUTRES cases de chaque
    // ligne qui passe par elle, format {r1, c1, r2, c2}.
    // r1 == -1 signale la fin (les cases ont entre 2 et 4 lignes).
    static const int LINE_PAIRS[3][3][4][4];

private:
    void checkWinCondition(int r, int c, Winner player);
};

#endif
