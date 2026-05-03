#include "MinimaxAI.h"
#include <algorithm>   // std::fill, std::max, std::min
#include <climits>     // INT_MIN, INT_MAX
#include <ctime>       // clock(), CLOCKS_PER_SEC
#include <iostream>

// Petit helper : convertit un budget en millisecondes en nombre de ticks d'horloge.
// CLOCKS_PER_SEC est typiquement 1000 (Linux) ou 1000 ms (Windows MinGW).
static clock_t msToTicks(int ms) {
    return (clock_t)((long long)ms * CLOCKS_PER_SEC / 1000);
}

MinimaxAI::MinimaxAI(Winner myColor_, Winner opponentColor_, int maxDepth_, int budgetTime_)
    : myColor(myColor_), opponentColor(opponentColor_), maxDepth(maxDepth_),
      currentIterationDepth(maxDepth_), budgetMs(budgetTime_),
      aborted(false), nodeCount(0),
      tt(TT_SIZE),
      bookEnabled(false), bookMaxPly(12),
      bookExtraDepth(4), bookExtraBudgetMs(5000) {
    // tt initialise a TT_EMPTY (bound=0, hash=0) par defaut grace au constructeur du vector
    resetSearchState();
}

void MinimaxAI::enableBook(const std::string& filename,
                           int bookMaxPly_,
                           int extraDepth,
                           int extraBudgetMs) {
    bookEnabled       = true;
    bookFilename      = filename;
    bookMaxPly        = bookMaxPly_;
    bookExtraDepth    = extraDepth;
    bookExtraBudgetMs = extraBudgetMs;
    book.load(filename);
}

void MinimaxAI::saveBook() const {
    if (!bookEnabled) return;
    book.save(bookFilename);
}

void MinimaxAI::resetSearchState() {
    for (int p = 0; p < MAX_PLY; ++p) {
        killerMoves[p][0] = GameMove{-1, -1};
        killerMoves[p][1] = GameMove{-1, -1};
    }
    for (int s = 0; s < 2; ++s)
        for (int c = 0; c < 81; ++c)
            historyTable[s][c] = 0;
}

bool MinimaxAI::timeExceeded() {
    if (budgetMs <= 0) return false;
    // clock() est cher : on ne le verifie qu'un noeud sur 4096
    if ((++nodeCount & 0xFFF) != 0) return aborted;
    if (clock() >= deadline) aborted = true;
    return aborted;
}

void MinimaxAI::clearTT() {
    std::fill(tt.begin(), tt.end(), TTEntry{0, 0, 0, TT_EMPTY, {-1, -1}});
}

GameMove MinimaxAI::findBestMove(UltimateBoard& board) {
    clock_t searchStart = clock();

    GameMove moves[81];
    int rootN = board.fillLegalMoves(moves);
    if (rootN == 0) return {0, 0};

    // Detection : un coup gagnait-il une sous-grille ? (avant de choisir)
    bool hadLocalWin = false;
    for (int i = 0; i < rootN; ++i) {
        int sR = moves[i].row / 3, sC = moves[i].col / 3;
        int lR = moves[i].row % 3, lC = moves[i].col % 3;
        if (wouldCompleteLine(board.getSubBoard(sR, sC), lR, lC, myColor)) {
            hadLocalWin = true;
            break;
        }
    }

    // ------ Lookup dans l'opening book : si on a deja une reponse, on coupe court ------
    uint64_t hash = board.getHash();
    if (bookEnabled) {
        GameMove bookMove;
        if (book.lookup(hash, bookMove) && board.isValidMove(bookMove)) {
            // Took-local-win pour le coup du livre
            int sR = bookMove.row / 3, sC = bookMove.col / 3;
            int lR = bookMove.row % 3, lC = bookMove.col % 3;
            bool tookWin = wouldCompleteLine(board.getSubBoard(sR, sC), lR, lC, myColor);

            lastInfo.bestMove        = bookMove;
            lastInfo.depthReached    = -1; // sentinelle "from book"
            lastInfo.score           = 0;
            lastInfo.elapsedMs       = 0;
            lastInfo.legalMovesCount = rootN;
            lastInfo.hadLocalWin     = hadLocalWin;
            lastInfo.tookLocalWin    = tookWin;
            return bookMove;
        }
    }

    // ------ Determination du regime de recherche : early-game (deep think) ou normal ------
    int moveCount = board.getMoveCount();
    bool earlyGame = bookEnabled && (moveCount < bookMaxPly);
    int effectiveMaxDepth = earlyGame ? (maxDepth + bookExtraDepth) : maxDepth;
    int effectiveBudgetMs = (budgetMs > 0)
                          ? (earlyGame ? (budgetMs + bookExtraBudgetMs) : budgetMs)
                          : 0;

    // Setup du budget de temps
    aborted = false;
    nodeCount = 0;
    if (effectiveBudgetMs > 0) {
        deadline = clock() + msToTicks(effectiveBudgetMs);
    }

    // Reset killers + history a chaque nouvelle position au root.
    // (Conserves entre iterations ID dans la meme recherche.)
    resetSearchState();

    // Resultat de la derniere iteration COMPLETEE (fallback si on est interrompu)
    GameMove bestMove = moves[0];
    int bestScore = 0;
    int reachedDepth = 0;

    // ------ Iterative deepening ------
    for (int curDepth = 1; curDepth <= effectiveMaxDepth; ++curDepth) {
        currentIterationDepth = curDepth;

        // On ordonne au root avec le meilleur coup connu (typiquement issu de l'iteration precedente,
        // recupere via la TT puisque ttStore a ete fait en fin d'iteration).
        GameMove ttMove{-1, -1};
        bool hasTT = false;
        {
            const TTEntry& e = tt[board.getHash() & TT_MASK];
            if (e.bound != TT_EMPTY && e.hash == board.getHash() && e.bestMove.row >= 0) {
                ttMove = e.bestMove;
                hasTT = true;
            }
        }
        orderMoves(board, moves, rootN, myColor, hasTT ? &ttMove : nullptr, 0);

        GameMove iterBestMove = moves[0];
        int iterBestScore = INT_MIN;
        int alpha = INT_MIN;
        int beta  = INT_MAX;

        for (int i = 0; i < rootN; ++i) {
            board.playMove(moves[i], myColor);
            int score = minimax(board, curDepth - 1, alpha, beta, false);
            board.undoMove();

            if (aborted) break; // recherche tronquee : on jette cette iteration

            if (score > iterBestScore) {
                iterBestScore = score;
                iterBestMove = moves[i];
            }
            alpha = std::max(alpha, iterBestScore);
        }

        if (aborted) break; // on garde le resultat de l'iteration precedente

        // Iteration completee : on commit
        bestMove  = iterBestMove;
        bestScore = iterBestScore;
        reachedDepth = curDepth;

        // On stocke le resultat du root dans la TT pour booster l'iteration suivante
        ttStore(board.getHash(), bestScore, curDepth, TT_EXACT, bestMove, 0);

        // Mat trouve : inutile de chercher plus profond
        if (bestScore >= MATE_THRESHOLD || bestScore <= -MATE_THRESHOLD) break;

        // Si la prochaine iteration n'a aucune chance de tenir dans le budget restant,
        // on coupe. Heuristique : la profondeur suivante coute 3-5x la courante.
        if (effectiveBudgetMs > 0) {
            clock_t remainingTicks = deadline - clock();
            long remainingMs = (long)((long long)remainingTicks * 1000 / CLOCKS_PER_SEC);
            if (remainingMs <= effectiveBudgetMs / 3) break;
        }
    }

    // Sauvegarde dans le livre si on est en early game
    if (earlyGame && bestMove.row >= 0 && bestMove.col >= 0) {
        book.insert(hash, bestMove);
    }

    // Remplir les infos de diagnostic
    long elapsedMs = (long)((long long)(clock() - searchStart) * 1000 / CLOCKS_PER_SEC);
    {
        int sR = bestMove.row / 3, sC = bestMove.col / 3;
        int lR = bestMove.row % 3, lC = bestMove.col % 3;
        bool tookWin = wouldCompleteLine(board.getSubBoard(sR, sC), lR, lC, myColor);
        lastInfo.bestMove        = bestMove;
        lastInfo.depthReached    = reachedDepth;
        lastInfo.score           = bestScore;
        lastInfo.elapsedMs       = elapsedMs;
        lastInfo.legalMovesCount = rootN;
        lastInfo.hadLocalWin     = hadLocalWin;
        lastInfo.tookLocalWin    = tookWin;
    }

    return bestMove;
}

// ============================================================
//                       MINIMAX + ALPHA-BETA + TT
// ============================================================

int MinimaxAI::minimax(UltimateBoard& board, int depth, int alpha, int beta, bool maximizing) {
    // Time control : si le budget est depasse, on remonte sans plus chercher.
    // La valeur retournee n'a pas d'importance car findBestMove jettera l'iteration.
    if (timeExceeded()) return 0;

    int plyFromRoot = currentIterationDepth - depth;
    int alphaOrig = alpha;
    int betaOrig  = beta;

    // Etats terminaux
    Winner w = board.getWinner();
    if (w == myColor)        return WIN_SCORE  - plyFromRoot;
    if (w == opponentColor)  return -WIN_SCORE + plyFromRoot;
    if (board.isTerminal()) {
        // Pas de gagnant 3-en-ligne mais board fini : tiebreaker par nombre
        // de sous-grilles. Le moteur du prof utilise probablement cette regle.
        int myCount  = board.countSubBoardsWon(myColor);
        int oppCount = board.countSubBoardsWon(opponentColor);
        if (myCount > oppCount) return  (WIN_SCORE - 100) - plyFromRoot;
        if (myCount < oppCount) return -(WIN_SCORE - 100) + plyFromRoot;
        return 0;
    }
    if (depth <= 0)          return evaluate(board, maximizing);

    uint64_t hash = board.getHash();

    // ----- TT probe -----
    GameMove ttMove{-1, -1};
    bool hasTTMove = false;
    {
        const TTEntry& e = tt[hash & TT_MASK];
        if (e.bound != TT_EMPTY && e.hash == hash) {
            if (e.bestMove.row >= 0) {
                ttMove = e.bestMove;
                hasTTMove = true;
            }
            if ((int)e.depth >= depth) {
                int s = adjustScoreFromTT(e.score, plyFromRoot);
                if (e.bound == TT_EXACT) return s;
                if (e.bound == TT_LOWER && s >= beta)  return s;
                if (e.bound == TT_UPPER && s <= alpha) return s;
            }
        }
    }

    // ----- Generation + ordre des coups (zero alloc) -----
    GameMove moves[81];
    int n = board.fillLegalMoves(moves);
    if (n == 0) return evaluate(board, maximizing);

    Winner sideToMove = maximizing ? myColor : opponentColor;
    orderMoves(board, moves, n, sideToMove, hasTTMove ? &ttMove : nullptr, plyFromRoot);

    // ----- Recherche -----
    int best;
    GameMove bestMove = moves[0];
    // Index pour killer/history : 0 = myColor (= maximizing), 1 = opponent
    int sideIdx = maximizing ? 0 : 1;

    if (maximizing) {
        best = INT_MIN;
        for (int i = 0; i < n; ++i) {
            board.playMove(moves[i], myColor);
            int score = minimax(board, depth - 1, alpha, beta, false);
            board.undoMove();
            if (score > best) {
                best = score;
                bestMove = moves[i];
            }
            if (best > alpha) alpha = best;
            if (beta <= alpha) {
                // Beta-cutoff : on enregistre killer + history pour booster les freres
                if (plyFromRoot < MAX_PLY) {
                    GameMove& k0 = killerMoves[plyFromRoot][0];
                    if (k0.row != moves[i].row || k0.col != moves[i].col) {
                        killerMoves[plyFromRoot][1] = k0;
                        killerMoves[plyFromRoot][0] = moves[i];
                    }
                }
                int cellIdx = moves[i].row * 9 + moves[i].col;
                historyTable[sideIdx][cellIdx] += depth * depth;
                break;
            }
        }
    } else {
        best = INT_MAX;
        for (int i = 0; i < n; ++i) {
            board.playMove(moves[i], opponentColor);
            int score = minimax(board, depth - 1, alpha, beta, true);
            board.undoMove();
            if (score < best) {
                best = score;
                bestMove = moves[i];
            }
            if (best < beta) beta = best;
            if (beta <= alpha) {
                if (plyFromRoot < MAX_PLY) {
                    GameMove& k0 = killerMoves[plyFromRoot][0];
                    if (k0.row != moves[i].row || k0.col != moves[i].col) {
                        killerMoves[plyFromRoot][1] = k0;
                        killerMoves[plyFromRoot][0] = moves[i];
                    }
                }
                int cellIdx = moves[i].row * 9 + moves[i].col;
                historyTable[sideIdx][cellIdx] += depth * depth;
                break;
            }
        }
    }

    // ----- TT store -----
    TTBound boundType;
    if      (best <= alphaOrig) boundType = TT_UPPER; // failed-low : borne sup
    else if (best >= betaOrig)  boundType = TT_LOWER; // failed-high : borne inf
    else                        boundType = TT_EXACT;
    ttStore(hash, best, depth, boundType, bestMove, plyFromRoot);

    return best;
}

// ============================================================
//                  TRANSPOSITION TABLE HELPERS
// ============================================================

void MinimaxAI::ttStore(uint64_t hash, int score, int depth, TTBound bound,
                        GameMove bestMove, int plyFromRoot) {
    TTEntry& e = tt[hash & TT_MASK];
    // Strategie de remplacement : on remplace si l'entree est vide,
    // si la position differe, ou si la nouvelle profondeur est >= a l'ancienne.
    if (e.bound == TT_EMPTY || e.hash != hash || depth >= (int)e.depth) {
        e.hash     = hash;
        e.score    = (int32_t)adjustScoreToTT(score, plyFromRoot);
        e.depth    = (int8_t)depth;
        e.bound    = (uint8_t)bound;
        e.bestMove = bestMove;
    }
}

// Correction "mate distance" : on stocke des scores independants de la distance au root,
// puis on les re-ajuste a la lecture. Sans ca, un mate cache risque d'etre interprete
// avec la mauvaise profondeur si la position est retrouvee ailleurs dans l'arbre.
int MinimaxAI::adjustScoreToTT(int score, int plyFromRoot) const {
    if (score >=  MATE_THRESHOLD) return score + plyFromRoot;
    if (score <= -MATE_THRESHOLD) return score - plyFromRoot;
    return score;
}

int MinimaxAI::adjustScoreFromTT(int score, int plyFromRoot) const {
    if (score >=  MATE_THRESHOLD) return score - plyFromRoot;
    if (score <= -MATE_THRESHOLD) return score + plyFromRoot;
    return score;
}

// ============================================================
//                       MOVE ORDERING
// ============================================================

void MinimaxAI::orderMoves(const UltimateBoard& board,
                           GameMove* moves, int n,
                           Winner player,
                           const GameMove* priorityMove,
                           int plyFromRoot) const {
    // Tableaux paralleles stack-alloues (pas de heap)
    int scores[81];

    int sideIdx = (player == myColor) ? 0 : 1;
    bool hasKillers = (plyFromRoot < MAX_PLY);
    GameMove k0 = hasKillers ? killerMoves[plyFromRoot][0] : GameMove{-1, -1};
    GameMove k1 = hasKillers ? killerMoves[plyFromRoot][1] : GameMove{-1, -1};

    for (int i = 0; i < n; ++i) {
        int s = scoreMove(board, moves[i], player);

        // Coup TT (issu d'une recherche precedente) : priorite max
        if (priorityMove
            && priorityMove->row == moves[i].row
            && priorityMove->col == moves[i].col) {
            s += 1000000;
        }

        // Killer moves : coups qui ont cause un cutoff au meme ply
        if (hasKillers) {
            if (k0.row == moves[i].row && k0.col == moves[i].col)      s += 800;
            else if (k1.row == moves[i].row && k1.col == moves[i].col) s += 400;
        }

        // History heuristic : score cumule des cutoffs causes par cette cellule (ce side)
        int cellIdx = moves[i].row * 9 + moves[i].col;
        s += historyTable[sideIdx][cellIdx] / 64;

        scores[i] = s;
    }

    // Tri par insertion : rapide pour n <= ~30 (cas typique en UTTT) et zero alloc
    for (int i = 1; i < n; ++i) {
        int s = scores[i];
        GameMove m = moves[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < s) {
            scores[j + 1] = scores[j];
            moves[j + 1]  = moves[j];
            --j;
        }
        scores[j + 1] = s;
        moves[j + 1]  = m;
    }
}

int MinimaxAI::scoreMove(const UltimateBoard& board,
                         const GameMove& m,
                         Winner player) const {
    int sR = m.row / 3, sC = m.col / 3;
    int lR = m.row % 3, lC = m.col % 3;
    Winner opp = (player == IA) ? PLAYER : IA;

    static const int globalPos[3][3] = {
        {3, 2, 3},
        {2, 4, 2},
        {3, 2, 3}
    };

    int score = 0;
    const SubBoard& localSb = board.getSubBoard(sR, sC);

    bool wins = wouldCompleteLine(localSb, lR, lC, player);
    if (wins) {
        score += 10000 + 200 * globalPos[sR][sC];
    }

    if (wouldCompleteLine(localSb, lR, lC, opp)) {
        score += 5000;
    }

    bool nextIsLocal = (sR == lR && sC == lC);
    if (nextIsLocal) {
        if (wins) score -= 4000; // on envoie l'adversaire en choix libre
    } else {
        const SubBoard& nextSb = board.getSubBoard(lR, lC);
        if (nextSb.isFinished()) {
            score -= 4000;
        } else if (subBoardHasOneMoveWin(nextSb, opp)) {
            score -= 2500;
        } else if (subBoardHasOneMoveWin(nextSb, player)) {
            score += 800;
        }
    }

    if (lR == 1 && lC == 1) score += 30;
    else if ((lR == 0 || lR == 2) && (lC == 0 || lC == 2)) score += 20;
    else score += 10;

    score += 5 * globalPos[sR][sC];

    return score;
}

bool MinimaxAI::wouldCompleteLine(const SubBoard& sb, int lR, int lC, Winner player) const {
    if (sb.getCell(lR, lC) != NO_WINNER) return false;

    bool line = true;
    for (int j = 0; j < 3; ++j) {
        if (j == lC) continue;
        if (sb.getCell(lR, j) != player) { line = false; break; }
    }
    if (line) return true;

    line = true;
    for (int i = 0; i < 3; ++i) {
        if (i == lR) continue;
        if (sb.getCell(i, lC) != player) { line = false; break; }
    }
    if (line) return true;

    if (lR == lC) {
        line = true;
        for (int i = 0; i < 3; ++i) {
            if (i == lR) continue;
            if (sb.getCell(i, i) != player) { line = false; break; }
        }
        if (line) return true;
    }

    if (lR + lC == 2) {
        line = true;
        for (int i = 0; i < 3; ++i) {
            int j = 2 - i;
            if (i == lR) continue;
            if (sb.getCell(i, j) != player) { line = false; break; }
        }
        if (line) return true;
    }

    return false;
}

bool MinimaxAI::subBoardHasOneMoveWin(const SubBoard& sb, Winner player) const {
    if (sb.isFinished()) return false;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (sb.getCell(r, c) != NO_WINNER) continue;
            if (wouldCompleteLine(sb, r, c, player)) return true;
        }
    }
    return false;
}

// ============================================================
//                       EVALUATION
// ============================================================

int MinimaxAI::evaluate(const UltimateBoard& board, bool meToMove) const {
    Winner globalWinner = board.getWinner();
    if (globalWinner == myColor) return WIN_SCORE;
    if (globalWinner == opponentColor) return -WIN_SCORE;

    static const int posBonus[3][3] = {
        {3, 2, 3},
        {2, 4, 2},
        {3, 2, 3}
    };

    int score = 0;

    // On encode les sous-grilles dans un tableau adapte a countTwoInLine.
    // Cas special : sous-grille pleine sans vainqueur (nulle) -> on la marque
    // IA_AND_PLAYER, qui sera vu comme un blocker pour les deux joueurs.
    Winner subWinners[3][3];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const SubBoard& sb = board.getSubBoard(r, c);
            Winner w = sb.getWinner();

            if (w == NO_WINNER && sb.isFinished()) {
                subWinners[r][c] = IA_AND_PLAYER; // nulle -> ligne morte
            } else {
                subWinners[r][c] = w;
            }

            if (w == myColor) {
                score += 65 * posBonus[r][c];   // (C re-tune) 80 -> 65
            } else if (w == opponentColor) {
                score -= 65 * posBonus[r][c];   // (C re-tune) 80 -> 65
            } else if (!sb.isFinished()) {
                score += evaluateSubBoard(sb, myColor);
                score -= evaluateSubBoard(sb, opponentColor);
            }
            // sous-grille nulle : 0 net (deja correct)
        }
    }

    // 2-en-ligne global : signal tactique principal (re-tune a la baisse,
    // la fork detection prend le relais comme moteur principal).
    int myThreats  = countTwoInLine(subWinners, myColor);
    int oppThreats = countTwoInLine(subWinners, opponentColor);
    score += 140 * myThreats;   // (C re-tune) 200 -> 140
    score -= 180 * oppThreats;  // (C re-tune) 250 -> 180

    // (B) Fork global : 2+ menaces 2-en-ligne au niveau global = quasi-mat.
    //     L'adversaire ne peut bloquer qu'une menace, on gagne sur l'autre.
    //     Asymetrie marquee : defendre un fork adverse est plus critique.
    if (myThreats  >= 2) score += 800;
    if (oppThreats >= 2) score -= 1000;

    // (D) Open lines : lignes encore gagnables, donne du gradient
    int myOpen  = countOpenLines(subWinners, myColor);
    int oppOpen = countOpenLines(subWinners, opponentColor);
    score += 25 * (myOpen - oppOpen);

    // ---- Facteur target : ou doit jouer le joueur au trait ? ----
    int targetR = board.getTargetRow();
    int targetC = board.getTargetCol();
    int sign = meToMove ? 1 : -1;

    if (targetR == -1) {
        score += sign * 60;
    } else {
        const SubBoard& target = board.getSubBoard(targetR, targetC);
        if (!target.isFinished()) {
            Winner self  = meToMove ? myColor : opponentColor;
            Winner other = meToMove ? opponentColor : myColor;
            if (subBoardHasOneMoveWin(target, self)) {
                score += sign * 90;   // (C re-tune) 120 -> 90
            }
            if (subBoardHasOneMoveWin(target, other)) {
                score -= sign * 50;   // (C re-tune) 70 -> 50
            }
        }
    }

    return score;
}

int MinimaxAI::countOpenLines(const Winner sw[3][3], Winner who) const {
    // Une ligne est "ouverte" pour `who` si aucune de ses 3 cases ne contient
    // d'adversaire (Winner != who && != NO_WINNER). Les sous-grilles nulles
    // (encodees IA_AND_PLAYER en amont) bloquent donc bien les deux joueurs.

    // On enumere les 8 lignes (3 rangees + 3 colonnes + 2 diagonales).
    // Tableau de 8 lignes x 3 cellules.
    Winner lines[8][3] = {
        { sw[0][0], sw[0][1], sw[0][2] },
        { sw[1][0], sw[1][1], sw[1][2] },
        { sw[2][0], sw[2][1], sw[2][2] },
        { sw[0][0], sw[1][0], sw[2][0] },
        { sw[0][1], sw[1][1], sw[2][1] },
        { sw[0][2], sw[1][2], sw[2][2] },
        { sw[0][0], sw[1][1], sw[2][2] },
        { sw[0][2], sw[1][1], sw[2][0] },
    };

    int count = 0;
    for (int i = 0; i < 8; ++i) {
        bool open = true;
        for (int j = 0; j < 3; ++j) {
            Winner x = lines[i][j];
            // Cellule "OK" si elle est a `who` ou vide. Sinon, ligne bloquee.
            if (x != who && x != NO_WINNER) { open = false; break; }
        }
        if (open) ++count;
    }
    return count;
}

int MinimaxAI::evaluateSubBoard(const SubBoard& sb, Winner who) const {
    Winner cells[3][3];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            cells[r][c] = sb.getCell(r, c);

    int score = 0;
    if (cells[1][1] == who) score += 3;
    if (cells[0][0] == who) score += 2;
    if (cells[0][2] == who) score += 2;
    if (cells[2][0] == who) score += 2;
    if (cells[2][2] == who) score += 2;

    int threats = countTwoInLine(cells, who);
    score += 5 * threats;
    // (B) Fork sous-grille : 2+ menaces simultanees -> quasi-victoire de la sous-grille
    //     L'adversaire ne peut bloquer qu'une menace par coup.
    if (threats >= 2) score += 40;

    return score;
}

int MinimaxAI::countTwoInLine(const Winner cells[3][3], Winner who) const {
    // Une "menace" 2-en-ligne = exactement 2 cases a `who`, 1 case vide,
    // ET aucune case "bloquante" (adversaire ou case nulle marquee IA_AND_PLAYER).

    // Meme structure que countOpenLines : on enumere les 8 lignes.
    Winner lines[8][3] = {
        { cells[0][0], cells[0][1], cells[0][2] },
        { cells[1][0], cells[1][1], cells[1][2] },
        { cells[2][0], cells[2][1], cells[2][2] },
        { cells[0][0], cells[1][0], cells[2][0] },
        { cells[0][1], cells[1][1], cells[2][1] },
        { cells[0][2], cells[1][2], cells[2][2] },
        { cells[0][0], cells[1][1], cells[2][2] },
        { cells[0][2], cells[1][1], cells[2][0] },
    };

    int total = 0;
    for (int i = 0; i < 8; ++i) {
        int mine = 0;
        bool blocked = false;
        for (int j = 0; j < 3; ++j) {
            Winner x = lines[i][j];
            if (x == who)            mine++;
            else if (x != NO_WINNER) blocked = true; // adversaire ou nulle
        }
        if (mine == 2 && !blocked) total++;
    }
    return total;
}
