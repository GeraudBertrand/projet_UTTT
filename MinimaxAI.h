#ifndef MINIMAXAI_H
#define MINIMAXAI_H

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include "UltimateBoard.h"
#include "OpeningBook.h"
#include "main.h"

class MinimaxAI {
public:
    // Info exposee apres chaque appel a findBestMove (pour logging / diagnostic)
    struct SearchInfo {
        GameMove bestMove;
        int      depthReached;     // profondeur ID complete la plus haute
        int      score;            // score retourne pour bestMove
        long     elapsedMs;        // duree de la recherche
        int      legalMovesCount;  // nb de coups legaux a la position
        bool     hadLocalWin;      // au moins un coup gagnait une sous-grille
        bool     tookLocalWin;     // le coup choisi gagne une sous-grille
    };

    MinimaxAI(Winner myColor, Winner opponentColor, int maxDepth = 5, int budgetTime = 0);

    GameMove findBestMove(UltimateBoard& board);

    const SearchInfo& getLastSearchInfo() const { return lastInfo; }

    void setDepth(int d) { maxDepth = d; }
    void setTimeBudgetMs(int ms) { budgetMs = ms; }
    void clearTT();

    // Active l'opening book : charge le fichier (s'il existe), et configure
    // une recherche plus profonde + plus de temps sur les premiers plies.
    // bookMaxPly : on n'enregistre que les positions a moins de N coups joues.
    // extraDepth/extraBudgetMs : ajoutes a maxDepth/budgetMs sur les positions early-game.
    void enableBook(const std::string& filename,
                    int bookMaxPly = 12,
                    int extraDepth = 4,
                    int extraBudgetMs = 5000);

    // Sauvegarde le livre. A appeler entre les parties pour ne rien perdre.
    void saveBook() const;

private:
    Winner myColor;
    Winner opponentColor;
    int maxDepth;

    static const int WIN_SCORE = 1000000;
    static const int MATE_THRESHOLD = WIN_SCORE - 1000;
    static const int MAX_PLY = 64;

    // Killer moves : 2 coups par ply qui ont provoque un beta-cutoff
    GameMove killerMoves[MAX_PLY][2];
    // History heuristic : score cumule par (joueur, cellule). Index 0 = myColor, 1 = opponent
    int historyTable[2][81];

    // -------- Iterative deepening / time control --------
    int currentIterationDepth;   // profondeur de l'iteration ID en cours (sert au plyFromRoot)
    int budgetMs;                // 0 = illimite
    std::chrono::steady_clock::time_point deadline;
    bool aborted;
    uint32_t nodeCount;          // pour throttler les checks d'horloge

    bool timeExceeded();

    // -------- Transposition Table --------
    enum TTBound : uint8_t { TT_EMPTY = 0, TT_EXACT = 1, TT_LOWER = 2, TT_UPPER = 3 };

    struct TTEntry {
        uint64_t hash;
        int32_t  score;
        int8_t   depth;
        uint8_t  bound;     // TTBound
        GameMove bestMove;
    };

    static const int TT_SIZE_LOG2 = 20;             // 2^20 = ~1M entrees
    static const int TT_SIZE      = 1 << TT_SIZE_LOG2;
    static const uint64_t TT_MASK = (uint64_t)TT_SIZE - 1;

    std::vector<TTEntry> tt;

    SearchInfo lastInfo;

    // -------- Opening Book --------
    OpeningBook  book;
    bool         bookEnabled;
    std::string  bookFilename;
    int          bookMaxPly;       // jusqu'a quel coup on alimente le livre
    int          bookExtraDepth;   // bonus de profondeur en early game
    int          bookExtraBudgetMs;// bonus de temps en early game

    void ttStore(uint64_t hash, int score, int depth, TTBound bound,
                 GameMove bestMove, int plyFromRoot);

    int adjustScoreToTT(int score, int plyFromRoot) const;
    int adjustScoreFromTT(int score, int plyFromRoot) const;

    // -------- Search --------
    int minimax(UltimateBoard& board, int depth, int alpha, int beta, bool maximizing);
    int evaluate(const UltimateBoard& board, bool meToMove) const;
    int evaluateSubBoard(const SubBoard& sb, Winner who) const;
    int countTwoInLine(const Winner cells[3][3], Winner who) const;
    // Lignes (au niveau global) encore gagnables par `who` : aucune cellule
    // adverse ni nulle (IA_AND_PLAYER). 8 lignes max (3 + 3 + 2).
    int countOpenLines(const Winner sw[3][3], Winner who) const;

    void resetSearchState(); // killers + history avant un nouveau findBestMove

    // -------- Move ordering (no allocations) --------
    void orderMoves(const UltimateBoard& board,
                    GameMove* moves, int n,
                    Winner player,
                    const GameMove* priorityMove,
                    int plyFromRoot) const;
    int scoreMove(const UltimateBoard& board,
                  const GameMove& m,
                  Winner player) const;
    bool wouldCompleteLine(const SubBoard& sb, int lR, int lC, Winner player) const;
    bool subBoardHasOneMoveWin(const SubBoard& sb, Winner player) const;
};

#endif
