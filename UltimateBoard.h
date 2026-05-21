#ifndef ULTIMATEBOARD_H
#define ULTIMATEBOARD_H

#include <cstdint>
#include "SubBoard.h"

// =============================================================================
//  UltimateBoard : le plateau Ultimate Tic-Tac-Toe complet.
//
//  C'est un 3x3 de SubBoard, plus les regles specifiques d'UTTT :
//   - le coup d'un joueur force l'adversaire a jouer dans une sous-grille
//     precise (celle indiquee par la position LOCALE de la case jouee) ;
//   - si cette sous-grille est terminee, l'adversaire a choix libre.
//
//  Etat maintenu :
//   - les 9 SubBoard
//   - la cible courante (targetSubRow/Col) ou -1 = choix libre
//   - le gagnant global, mis a jour incrementalement
//   - un hash Zobrist incremental pour la transposition table de l'IA
//   - une pile d'undo pour permettre au minimax de jouer/defaire les coups
// =============================================================================
class UltimateBoard {
private:
    // Une entree de la pile d'undo : tout ce qu'il faut pour annuler proprement
    // un playMove(). On enregistre l'etat AVANT le coup.
    struct UndoInfo {
        GameMove move;                // le coup qui a ete joue
        int      prevTargetRow;       // cible imposee avant le coup
        int      prevTargetCol;
        Winner   prevOverallWinner;   // gagnant global avant le coup
        Winner   prevSubBoardWinner;  // gagnant de la sous-grille avant le coup
        uint64_t prevHash;            // hash Zobrist avant le coup
    };

    SubBoard subBoards[3][3];
    int      targetSubRow;
    int      targetSubCol;
    Winner   overallWinner;

    // Pile d'undo en taille fixe : une partie d'UTTT fait au plus 81 coups.
    UndoInfo history[81];
    int      historySize;

    // Hash Zobrist maintenu de maniere incrementale a chaque playMove/undoMove.
    uint64_t zobristHash;

public:
    UltimateBoard();

    // -------- Regles de jeu / mutations --------
    bool isValidMove(GameMove m) const;
    void playMove(GameMove m, Winner player);
    void undoMove();

    // Remplit `out` avec les coups legaux et retourne leur nombre.
    // Aucune allocation : on parcourt directement la sous-grille forcee
    // si target != -1, sinon toutes les sous-grilles encore ouvertes.
    int fillLegalMoves(GameMove out[81]) const;

    // -------- Queries --------
    bool isTerminal() const;
    int  getTargetRow() const { return targetSubRow; }
    int  getTargetCol() const { return targetSubCol; }
    Winner getWinner() const  { return overallWinner; }
    const SubBoard& getSubBoard(int r, int c) const { return subBoards[r][c]; }
    uint64_t getHash() const { return zobristHash; }

    // Compte les sous-grilles gagnees par `w`.
    // Sert de tiebreaker en fin de partie (si aucun 3-en-ligne global).
    int countSubBoardsWon(Winner w) const;

    // Nombre de coups deja joues (= taille de la pile d'undo).
    int getMoveCount() const { return historySize; }

private:
    // Met a jour overallWinner uniquement en regardant les lignes qui passent
    // par (changedSR, changedSC) : seule cette sous-grille vient de changer,
    // donc seules 2 a 4 lignes globales peuvent etre completees.
    void updateOverallWinner(int changedSR, int changedSC);

    // ---- Tables Zobrist : initialisees une seule fois, partagees globalement ----
    static uint64_t zCells[81][2];   // [cellule 0..80][0 = IA, 1 = PLAYER]
    static uint64_t zTarget[10];     // 0..8 = sous-grille forcee, 9 = choix libre
    static uint64_t zSide;            // XOR a chaque coup pour suivre le trait
    static bool     zInitialized;
    static void initZobrist();
};

#endif
