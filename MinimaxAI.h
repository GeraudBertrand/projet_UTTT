#ifndef MINIMAXAI_H
#define MINIMAXAI_H

#include <cstdint>
#include <ctime>      // clock(), clock_t, CLOCKS_PER_SEC
#include <string>
#include <vector>
#include "UltimateBoard.h"
#include "OpeningBook.h"
#include "main.h"

// =============================================================================
//  MinimaxAI : moteur de decision pour UTTT.
//
//  Briques :
//   - minimax + alpha-beta + iterative deepening
//   - transposition table (Zobrist hash + mate-distance correction)
//   - move ordering : coup TT, killer moves, history heuristic, scoreMove rapide
//   - opening book optionnel (positions de debut de partie pre-calculees)
//   - budget de temps configurable (avec mode "deep think" pour l'opening)
//   - eval positionnelle : sous-grilles gagnees, menaces 2-en-ligne, forks,
//     lignes ouvertes, facteur "ou j'envoie l'adversaire"
//
//  L'IA expose un SearchInfo apres chaque recherche pour le logging.
// =============================================================================
class MinimaxAI {
public:
    // -------- Info de diagnostic exposee apres chaque findBestMove --------
    struct SearchInfo {
        GameMove bestMove;
        int      depthReached;     // profondeur ID atteinte (-1 si reponse du livre)
        int      score;            // score eval pour bestMove
        long     elapsedMs;        // duree totale de la recherche
        int      legalMovesCount;  // nb de coups legaux a la position
        bool     hadLocalWin;      // un coup gagnait une sous-grille ?
        bool     tookLocalWin;     // le coup choisi gagne une sous-grille ?
    };

    MinimaxAI(Winner myColor, Winner opponentColor, int maxDepth = 5, int budgetTime = 0);

    // -------- API principale --------
    GameMove findBestMove(UltimateBoard& board);
    const SearchInfo& getLastSearchInfo() const { return lastInfo; }

    // -------- Configuration --------
    void setDepth(int d)         { maxDepth = d; }
    void setTimeBudgetMs(int ms) { budgetMs = ms; }
    void clearTT();

    // Active l'opening book : charge le fichier (s'il existe), et configure
    // une recherche plus profonde + plus de temps sur les premiers plies.
    //   bookMaxPly     : on n'alimente le livre que tant qu'on est en-dessous.
    //   extraDepth     : ajoute a maxDepth en early game.
    //   extraBudgetMs  : ajoute a budgetMs en early game (= "deep think").
    void enableBook(const std::string& filename,
                    int bookMaxPly = 12,
                    int extraDepth = 4,
                    int extraBudgetMs = 5000);

    // A appeler entre les parties pour ne rien perdre du livre.
    void saveBook() const;

private:
    // -------- Identite / parametres --------
    Winner myColor;
    Winner opponentColor;
    int    maxDepth;

    // -------- Constantes de score --------
    static const int WIN_SCORE      = 1000000;       // valeur d'un mat
    static const int MATE_THRESHOLD = WIN_SCORE - 1000; // au-dela = mat detecte
    static const int MAX_PLY        = 64;            // borne sup. des killer/history

    // -------- Killer moves / history heuristic --------
    // Killers : 2 coups par ply qui ont provoque un beta-cutoff.
    GameMove killerMoves[MAX_PLY][2];
    // History : score cumule par (joueur, cellule).
    //   index 0 = myColor, 1 = opponent
    int historyTable[2][81];

    // -------- Iterative deepening + time control --------
    int     currentIterationDepth;  // profondeur de l'iteration ID en cours
    int     budgetMs;               // 0 = pas de limite de temps
    clock_t deadline;               // tick d'horloge auquel on arrete
    bool    aborted;
    unsigned int nodeCount;         // pour throttler les checks d'horloge

    bool timeExceeded();

    // -------- Transposition Table --------
    enum TTBound { TT_EMPTY = 0, TT_EXACT = 1, TT_LOWER = 2, TT_UPPER = 3 };

    struct TTEntry {
        uint64_t hash;
        int      score;
        int      depth;
        int      bound;     // valeur de l'enum TTBound
        GameMove bestMove;
    };

    static const int      TT_SIZE_LOG2 = 18;                       // 2^18 = ~256K entrees
    static const int      TT_SIZE      = 1 << TT_SIZE_LOG2;
    static const uint64_t TT_MASK      = (uint64_t)TT_SIZE - 1;

    std::vector<TTEntry> tt;

    void ttStore(uint64_t hash, int score, int depth, TTBound bound,
                 GameMove bestMove, int plyFromRoot);

    // Correction "mate distance" : transforme un score relatif a la racine
    // en score independant pour le stockage en TT, et inversement.
    int adjustScoreToTT(int score, int plyFromRoot) const;
    int adjustScoreFromTT(int score, int plyFromRoot) const;

    // -------- Diagnostic --------
    SearchInfo lastInfo;

    // -------- Opening Book --------
    OpeningBook book;
    bool        bookEnabled;
    std::string bookFilename;
    int         bookMaxPly;        // alimente le livre tant que move_count < bookMaxPly
    int         bookExtraDepth;    // bonus de profondeur en early game
    int         bookExtraBudgetMs; // bonus de budget temps en early game

    // -------- Recherche --------
    int minimax(UltimateBoard& board, int depth, int alpha, int beta, bool maximizing);

    // -------- Evaluation --------
    int evaluate(const UltimateBoard& board, bool meToMove) const;
    int evaluateSubBoard(const SubBoard& sb, Winner who) const;

    // Compte les menaces 2-en-ligne pour `who` (2 cases, 1 vide, pas de bloqueur).
    int countTwoInLine(const Winner cells[3][3], Winner who) const;

    // Compte les lignes globales encore gagnables pour `who` (aucune cellule
    // adverse ni nulle / IA_AND_PLAYER). Maximum : 8.
    int countOpenLines(const Winner sw[3][3], Winner who) const;

    // Reset killers + history avant un nouveau findBestMove.
    void resetSearchState();

    // -------- Move ordering (sans allocation) --------
    void orderMoves(const UltimateBoard& board,
                    GameMove* moves, int n,
                    Winner player,
                    const GameMove* priorityMove,
                    int plyFromRoot) const;
    int  scoreMove(const UltimateBoard& board,
                   const GameMove& m,
                   Winner player) const;
    // Note : wouldCompleteLine et hasOneMoveWin sont des methodes de SubBoard
    // (cf. SubBoard.h). On les appelle via sb.wouldCompleteLine(...) etc.
};

#endif
