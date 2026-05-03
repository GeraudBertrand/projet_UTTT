#ifndef ULTIMATEBOARD_H
#define ULTIMATEBOARD_H

#include <cstdint>
#include <vector>
#include "SubBoard.h"

class UltimateBoard {
private:
    struct UndoInfo {
        GameMove move;
        int prevTargetRow;
        int prevTargetCol;
        Winner prevOverallWinner;
        Winner prevSubBoardWinner;
        uint64_t prevHash;
    };

    SubBoard subBoards[3][3];
    int targetSubRow;
    int targetSubCol;
    Winner overallWinner;

    // Pile d'undo en taille fixe : une partie ne fait jamais plus de 81 coups
    UndoInfo history[81];
    int historySize;

    // Hash Zobrist maintenu incrementalement
    uint64_t zobristHash;

public:
    UltimateBoard();

    bool isValidMove(GameMove m) const;
    void playMove(GameMove m, Winner player);
    void undoMove();

    // API rapide pour le hot path minimax (zero allocation)
    // Retourne le nombre de coups, ecrits dans out[0..n-1]
    int fillLegalMoves(GameMove out[81]) const;

    // API legacy (allocation) - conservee pour code externe
    std::vector<GameMove> getLegalMoves() const;

    bool isTerminal() const;

    int getTargetRow() const { return targetSubRow; }
    int getTargetCol() const { return targetSubCol; }
    Winner getWinner() const { return overallWinner; }
    const SubBoard& getSubBoard(int r, int c) const { return subBoards[r][c]; }
    uint64_t getHash() const { return zobristHash; }

    // Compte les sous-grilles gagnees par `w`. Sert de tiebreaker en fin de partie
    // si aucun joueur n'a fait 3-en-ligne au niveau global.
    int countSubBoardsWon(Winner w) const;

    // Nombre de coups deja joues (= taille de la pile d'undo)
    int getMoveCount() const { return historySize; }

private:
    void updateOverallWinner();

    // Tables Zobrist (initialisees une seule fois, partagees entre instances)
    static uint64_t zCells[81][2];   // [cellule 0..80][0=IA, 1=PLAYER]
    static uint64_t zTarget[10];     // 0..8 = sous-grille forcee, 9 = choix libre
    static uint64_t zSide;            // XOR a chaque coup pour le trait
    static bool zInitialized;
    static void initZobrist();
};

#endif
