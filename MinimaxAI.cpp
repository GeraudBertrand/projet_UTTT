#include "MinimaxAI.h"
#include <algorithm>   // std::fill
#include <climits>     // INT_MIN, INT_MAX
#include <ctime>       // clock(), CLOCKS_PER_SEC
#include <iostream>

// =============================================================================
//  Helpers locaux
// =============================================================================

// Convertit un budget en millisecondes en nombre de ticks d'horloge.
// CLOCKS_PER_SEC varie selon la plateforme (1 000 000 sous Linux, 1000 sous
// Windows/MinGW). On utilise long long pour eviter l'overflow.
static clock_t msToTicks(int ms) {
    return (clock_t)((long long)ms * CLOCKS_PER_SEC / 1000);
}

// =============================================================================
//  Cycle de vie + reset
// =============================================================================

MinimaxAI::MinimaxAI(Winner myColor_, Winner opponentColor_, int maxDepth_, int budgetTime_)
    : myColor(myColor_), opponentColor(opponentColor_), maxDepth(maxDepth_),
      currentIterationDepth(maxDepth_), budgetMs(budgetTime_),
      aborted(false), nodeCount(0),
      tt(TT_SIZE),
      bookEnabled(false), bookMaxPly(12),
      bookExtraDepth(4), bookExtraBudgetMs(5000) {
    // tt est initialisee avec des entrees TT_EMPTY grace au constructeur du vector.
    resetSearchState();
}

// Reinitialise killers + history. Appele avant chaque nouvelle recherche
// (= avant chaque coup) ; les valeurs sont conservees entre les iterations
// d'iterative deepening DANS la meme recherche.
void MinimaxAI::resetSearchState() {
    for (int p = 0; p < MAX_PLY; ++p) {
        killerMoves[p][0] = GameMove{-1, -1};
        killerMoves[p][1] = GameMove{-1, -1};
    }
    for (int s = 0; s < 2; ++s)
        for (int c = 0; c < 81; ++c)
            historyTable[s][c] = 0;
}

void MinimaxAI::clearTT() {
    std::fill(tt.begin(), tt.end(), TTEntry{0, 0, 0, TT_EMPTY, {-1, -1}});
}

// =============================================================================
//  Opening book : activation / sauvegarde
// =============================================================================

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

// =============================================================================
//  Time control
// =============================================================================

// Verifie si on a depasse le deadline. Appel a clock() est cher : on ne le
// fait qu'un noeud sur 4096 (test sur les 12 bits de poids faible de nodeCount).
bool MinimaxAI::timeExceeded() {
    if (budgetMs <= 0) return false;
    if ((++nodeCount & 0xFFF) != 0) return aborted;
    if (clock() >= deadline) aborted = true;
    return aborted;
}

// =============================================================================
//  findBestMove : point d'entree depuis le manager
// =============================================================================
//
//  Sequence :
//   1) Generer les coups legaux a la racine.
//   2) Si l'opening book a une reponse pour cette position, la jouer direct.
//   3) Determiner le regime de recherche (early game = deep think, sinon
//      regime normal).
//   4) Iterative deepening : depth 1, 2, 3, ... jusqu'au plafond ou timeout.
//   5) Eventuellement ecrire le coup choisi dans le livre.
//   6) Remplir SearchInfo pour le logging.
//
// =============================================================================
GameMove MinimaxAI::findBestMove(UltimateBoard& board) {
    clock_t searchStart = clock();

    // -- (1) Coups legaux a la racine ----------------------------------------
    GameMove moves[81];
    int rootN = board.fillLegalMoves(moves);
    if (rootN == 0) return {0, 0}; // securite : aucun coup possible

    // -- Diagnostic : un coup gagnait-il une sous-grille ? -------------------
    bool hadLocalWin = false;
    for (int i = 0; i < rootN; ++i) {
        int sR = moves[i].row / 3, sC = moves[i].col / 3;
        int lR = moves[i].row % 3, lC = moves[i].col % 3;
        if (board.getSubBoard(sR, sC).wouldCompleteLine(lR, lC, myColor)) {
            hadLocalWin = true;
            break;
        }
    }

    // -- (2) Opening book : reponse instantanee si position connue -----------
    uint64_t hash = board.getHash();
    if (bookEnabled) {
        GameMove bookMove;
        if (book.lookup(hash, bookMove) && board.isValidMove(bookMove)) {
            int sR = bookMove.row / 3, sC = bookMove.col / 3;
            int lR = bookMove.row % 3, lC = bookMove.col % 3;
            bool tookWin = board.getSubBoard(sR, sC).wouldCompleteLine(lR, lC, myColor);

            lastInfo.bestMove        = bookMove;
            lastInfo.depthReached    = -1; // sentinelle "reponse du livre"
            lastInfo.score           = 0;
            lastInfo.elapsedMs       = 0;
            lastInfo.legalMovesCount = rootN;
            lastInfo.hadLocalWin     = hadLocalWin;
            lastInfo.tookLocalWin    = tookWin;
            return bookMove;
        }
    }

    // -- (3) Regime de recherche : early-game ou normal ----------------------
    int  moveCount       = board.getMoveCount();
    bool earlyGame       = bookEnabled && (moveCount < bookMaxPly);
    int  effectiveMaxDepth = earlyGame ? (maxDepth + bookExtraDepth) : maxDepth;
    int  effectiveBudgetMs = (budgetMs > 0)
                           ? (earlyGame ? (budgetMs + bookExtraBudgetMs) : budgetMs)
                           : 0;

    // Setup du time control
    aborted   = false;
    nodeCount = 0;
    if (effectiveBudgetMs > 0) {
        deadline = clock() + msToTicks(effectiveBudgetMs);
    }

    // Reset killers + history pour cette nouvelle position
    resetSearchState();

    // Resultat de la derniere iteration COMPLETEE (fallback si timeout)
    GameMove bestMove     = moves[0];
    int      bestScore    = 0;
    int      reachedDepth = 0;

    // -- (4) Iterative deepening --------------------------------------------
    // Chaque iteration profite du remplissage de la TT par la precedente :
    // le best move trouve a depth d-1 est essaye en premier a depth d,
    // ce qui maximise les coupures alpha-beta.
    for (int curDepth = 1; curDepth <= effectiveMaxDepth; ++curDepth) {
        currentIterationDepth = curDepth;

        // Recupere le best move connu pour cette position (de l'iter precedente)
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

        // Boucle sur les coups racines de cette iteration
        GameMove iterBestMove  = moves[0];
        int      iterBestScore = INT_MIN;
        int      alpha = INT_MIN;
        int      beta  = INT_MAX;

        for (int i = 0; i < rootN; ++i) {
            board.playMove(moves[i], myColor);
            int score = minimax(board, curDepth - 1, alpha, beta, false);
            board.undoMove();

            if (aborted) break; // recherche tronquee : on jette cette iteration

            if (score > iterBestScore) {
                iterBestScore = score;
                iterBestMove  = moves[i];
            }
            if (iterBestScore > alpha) alpha = iterBestScore;
        }

        if (aborted) break; // garde le resultat de l'iteration precedente

        // Iteration completee : on commit
        bestMove     = iterBestMove;
        bestScore    = iterBestScore;
        reachedDepth = curDepth;

        // Stocker le resultat du root dans la TT pour booster l'iteration suivante
        ttStore(board.getHash(), bestScore, curDepth, TT_EXACT, bestMove, 0);

        // Mat trouve : inutile de chercher plus profond
        if (bestScore >= MATE_THRESHOLD || bestScore <= -MATE_THRESHOLD) break;

        // Si la prochaine iteration n'a aucune chance de tenir dans le budget
        // restant, on coupe (heuristique : depth+1 coute typiquement 3-5x).
        if (effectiveBudgetMs > 0) {
            clock_t remainingTicks = deadline - clock();
            long    remainingMs    = (long)((long long)remainingTicks * 1000 / CLOCKS_PER_SEC);
            if (remainingMs <= effectiveBudgetMs / 3) break;
        }
    }

    // -- (5) Sauvegarde dans le livre si early game --------------------------
    if (earlyGame && bestMove.row >= 0 && bestMove.col >= 0) {
        book.insert(hash, bestMove);
    }

    // -- (6) Remplir SearchInfo pour le logging ------------------------------
    long elapsedMs = (long)((long long)(clock() - searchStart) * 1000 / CLOCKS_PER_SEC);
    {
        int sR = bestMove.row / 3, sC = bestMove.col / 3;
        int lR = bestMove.row % 3, lC = bestMove.col % 3;
        bool tookWin = board.getSubBoard(sR, sC).wouldCompleteLine(lR, lC, myColor);
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

// =============================================================================
//  minimax : recursion alpha-beta avec sondage/ecriture TT
// =============================================================================
//
//  Retourne le score de la position (du point de vue de myColor) en jouant
//  une recherche minimax de profondeur `depth` avec coupures alpha-beta.
//
//  La TT sert a 2 choses :
//   - rendre directement un score si la position a deja ete cherchee au moins
//     aussi profondement et que les bornes le permettent ;
//   - sinon, fournir le best move precedent pour le move ordering.
//
//  En cas de beta-cutoff, on met a jour killers + history pour booster
//  l'ordre des coups dans les noeuds freres au meme ply.
// =============================================================================
int MinimaxAI::minimax(UltimateBoard& board, int depth, int alpha, int beta, bool maximizing) {
    // Time control : on remonte sans plus chercher si on est en panne de temps.
    // La valeur retournee n'a pas d'importance, findBestMove jettera l'iteration.
    if (timeExceeded()) return 0;

    int plyFromRoot = currentIterationDepth - depth;
    int alphaOrig   = alpha;
    int betaOrig    = beta;

    // -- Etats terminaux ----------------------------------------------------
    Winner w = board.getWinner();
    if (w == myColor)       return  WIN_SCORE - plyFromRoot; // gagne tot > gagne tard
    if (w == opponentColor) return -WIN_SCORE + plyFromRoot;
    if (board.isTerminal()) {
        // Plateau plein sans 3-en-ligne global -> tiebreaker par nombre de
        // sous-grilles gagnees (regle utilisee par le moteur du prof).
        int myCount  = board.countSubBoardsWon(myColor);
        int oppCount = board.countSubBoardsWon(opponentColor);
        if (myCount > oppCount) return  (WIN_SCORE - 100) - plyFromRoot;
        if (myCount < oppCount) return -(WIN_SCORE - 100) + plyFromRoot;
        return 0;
    }
    if (depth <= 0) return evaluate(board, maximizing);

    uint64_t hash = board.getHash();

    // -- TT probe -----------------------------------------------------------
    GameMove ttMove{-1, -1};
    bool hasTTMove = false;
    {
        const TTEntry& e = tt[hash & TT_MASK];
        if (e.bound != TT_EMPTY && e.hash == hash) {
            if (e.bestMove.row >= 0) {
                ttMove = e.bestMove;
                hasTTMove = true;
            }
            // Entree assez profonde -> on peut potentiellement renvoyer son score.
            if (e.depth >= depth) {
                int s = adjustScoreFromTT(e.score, plyFromRoot);
                if (e.bound == TT_EXACT)                  return s;
                if (e.bound == TT_LOWER && s >= beta)     return s;
                if (e.bound == TT_UPPER && s <= alpha)    return s;
            }
        }
    }

    // -- Generation + ordre des coups (zero allocation) ---------------------
    GameMove moves[81];
    int n = board.fillLegalMoves(moves);
    if (n == 0) return evaluate(board, maximizing);

    Winner sideToMove = maximizing ? myColor : opponentColor;
    orderMoves(board, moves, n, sideToMove, hasTTMove ? &ttMove : nullptr, plyFromRoot);

    // -- Boucle alpha-beta ---------------------------------------------------
    int      best;
    GameMove bestMove = moves[0];
    int      sideIdx  = maximizing ? 0 : 1; // index pour killers/history

    if (maximizing) {
        best = INT_MIN;
        for (int i = 0; i < n; ++i) {
            board.playMove(moves[i], myColor);
            int score = minimax(board, depth - 1, alpha, beta, false);
            board.undoMove();
            if (score > best) { best = score; bestMove = moves[i]; }
            if (best > alpha) alpha = best;
            if (beta <= alpha) {
                // Beta-cutoff : on enregistre killer + history pour booster
                // l'ordre des coups dans les noeuds freres.
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
            if (score < best) { best = score; bestMove = moves[i]; }
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

    // -- TT store ------------------------------------------------------------
    // Type de borne en fonction du resultat par rapport aux bornes d'entree :
    //   - best <= alphaOrig : failed-low  -> borne SUPERIEURE (UPPER)
    //   - best >= betaOrig  : failed-high -> borne INFERIEURE (LOWER)
    //   - sinon             : valeur EXACTe dans la fenetre
    TTBound boundType;
    if      (best <= alphaOrig) boundType = TT_UPPER;
    else if (best >= betaOrig)  boundType = TT_LOWER;
    else                        boundType = TT_EXACT;
    ttStore(hash, best, depth, boundType, bestMove, plyFromRoot);

    return best;
}

// =============================================================================
//  Transposition table : store + correction de mate distance
// =============================================================================

// Strategie de remplacement : on ecrase l'entree existante si elle est vide,
// si la position differe (collision), ou si la nouvelle profondeur est >=.
// On garde donc les analyses les plus profondes.
void MinimaxAI::ttStore(uint64_t hash, int score, int depth, TTBound bound,
                        GameMove bestMove, int plyFromRoot) {
    TTEntry& e = tt[hash & TT_MASK];
    if (e.bound == TT_EMPTY || e.hash != hash || depth >= e.depth) {
        e.hash     = hash;
        e.score    = adjustScoreToTT(score, plyFromRoot);
        e.depth    = depth;
        e.bound    = bound;
        e.bestMove = bestMove;
    }
}

// Correction "mate distance" : un score de mat est intrinsequement lie a la
// distance au noeud racine de la recherche en cours. Si on stocke tel quel,
// on ne peut pas reutiliser l'entree quand on rencontre la position a une
// autre distance. Solution classique : on stocke un score "ajuste" qui ne
// depend plus que de la position, puis on le reconvertit a la lecture.
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

// =============================================================================
//  Move ordering
// =============================================================================
//
//  Plus on met les bons coups en debut de tableau, plus alpha-beta coupe tot.
//  Sources de signal (additionnees) :
//    - score statique du coup (scoreMove) : tactique locale
//    - coup TT (de l'iteration ou de la recherche precedente) : prioritaire
//    - killer moves au meme ply : ont deja cause un cutoff dans un frere
//    - history heuristic : statistique cumulative sur le run en cours
//
//  Tri par insertion : zero allocation, optimal pour n <= ~30 (cas UTTT).
// =============================================================================
void MinimaxAI::orderMoves(const UltimateBoard& board,
                           GameMove* moves, int n,
                           Winner player,
                           const GameMove* priorityMove,
                           int plyFromRoot) const {
    int scores[81]; // stack-alloue, pas de heap

    int  sideIdx    = (player == myColor) ? 0 : 1;
    bool hasKillers = (plyFromRoot < MAX_PLY);
    GameMove k0 = hasKillers ? killerMoves[plyFromRoot][0] : GameMove{-1, -1};
    GameMove k1 = hasKillers ? killerMoves[plyFromRoot][1] : GameMove{-1, -1};

    // 1) Compute scores
    for (int i = 0; i < n; ++i) {
        int s = scoreMove(board, moves[i], player);

        // Coup TT : priorite massive
        if (priorityMove
            && priorityMove->row == moves[i].row
            && priorityMove->col == moves[i].col) {
            s += 1000000;
        }
        // Killer moves au meme ply
        if (hasKillers) {
            if      (k0.row == moves[i].row && k0.col == moves[i].col) s += 800;
            else if (k1.row == moves[i].row && k1.col == moves[i].col) s += 400;
        }
        // History heuristic (echelle : /64 pour rester dans le meme ordre que les bonus)
        int cellIdx = moves[i].row * 9 + moves[i].col;
        s += historyTable[sideIdx][cellIdx] / 64;

        scores[i] = s;
    }

    // 2) Tri par insertion (n petit, donc O(n^2) est plus rapide que std::sort)
    for (int i = 1; i < n; ++i) {
        int      s = scores[i];
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

// Score statique d'un coup pour le move ordering. Capture les principaux
// signaux tactiques rapides a calculer (sans appel recursif au minimax) :
//   - gagner la sous-grille locale (gros bonus)
//   - bloquer une victoire adverse dans cette sous-grille
//   - eviter d'envoyer l'adversaire en bonne posture (cible apres notre coup)
//   - bonus de position (centre > coins > bords)
int MinimaxAI::scoreMove(const UltimateBoard& board,
                         const GameMove& m,
                         Winner player) const {
    int sR = m.row / 3, sC = m.col / 3;
    int lR = m.row % 3, lC = m.col % 3;
    Winner opp = (player == IA) ? PLAYER : IA;

    // Bonus de position globale (la sous-grille ou on joue est-elle "centrale" ?)
    static const int globalPos[3][3] = {
        {3, 2, 3},
        {2, 4, 2},
        {3, 2, 3}
    };

    int score = 0;
    const SubBoard& localSb = board.getSubBoard(sR, sC);

    // (a) Gagner la sous-grille locale
    bool wins = localSb.wouldCompleteLine(lR, lC, player);
    if (wins) {
        score += 10000 + 200 * globalPos[sR][sC];
    }
    // (b) Bloquer une victoire adverse imminente sur la meme case
    if (localSb.wouldCompleteLine(lR, lC, opp)) {
        score += 5000;
    }

    // (c) Effet sur la cible : ou envoie-t-on l'adversaire ?
    // Cas particulier : le coup atterrit dans la sous-grille qu'il cible.
    bool nextIsLocal = (sR == lR && sC == lC);
    if (nextIsLocal) {
        // Si on gagne la sous-grille ici, l'adversaire sera envoye dedans
        // (terminee) -> choix libre pour lui, mauvais.
        if (wins) score -= 4000;
    } else {
        const SubBoard& nextSb = board.getSubBoard(lR, lC);
        if (nextSb.isFinished()) {
            // Choix libre pour l'adversaire = potentiellement dangereux
            score -= 4000;
        } else if (nextSb.hasOneMoveWin(opp)) {
            // On l'envoie dans une sous-grille ou il peut gagner en 1 coup
            score -= 2500;
        } else if (nextSb.hasOneMoveWin(player)) {
            // On l'envoie dans une sous-grille ou WE pouvons gagner en 1 coup
            // (utile si on rejoue ensuite via free choice)
            score += 800;
        }
    }

    // (d) Position locale : centre > coins > bords
    if      (lR == 1 && lC == 1)                            score += 30;
    else if ((lR == 0 || lR == 2) && (lC == 0 || lC == 2))  score += 20;
    else                                                    score += 10;

    // (e) Bonus de position globale (recompense les sous-grilles "fortes")
    score += 5 * globalPos[sR][sC];

    return score;
}

// =============================================================================
//  Evaluation (heuristique statique en feuille de l'arbre)
// =============================================================================
//
//  Ingredients :
//   - sous-grilles gagnees, ponderees par position globale
//   - menaces 2-en-ligne au niveau global (avec fork detection)
//   - lignes globales encore ouvertes (gradient)
//   - facteur "trait" : ou est envoye le joueur au trait ?
//   - eval interne pour les sous-grilles non terminees (positions des pions,
//     menaces 2-en-ligne, mini-forks)
//
//  Tous les scores sont du point de vue de myColor.
// =============================================================================
int MinimaxAI::evaluate(const UltimateBoard& board, bool meToMove) const {
    Winner globalWinner = board.getWinner();
    if (globalWinner == myColor)       return  WIN_SCORE;
    if (globalWinner == opponentColor) return -WIN_SCORE;

    static const int posBonus[3][3] = {
        {3, 2, 3},
        {2, 4, 2},
        {3, 2, 3}
    };

    int score = 0;

    // --- (1) Etat des 9 sous-grilles ----------------------------------------
    // On encode dans un tableau adapte aux fonctions de comptage de lignes.
    // Cas special : sous-grille pleine sans gagnant (nulle) -> IA_AND_PLAYER,
    // qui est vu comme un bloqueur pour les deux joueurs.
    Winner subWinners[3][3];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const SubBoard& sb = board.getSubBoard(r, c);
            Winner w = sb.getWinner();

            if (w == NO_WINNER && sb.isFinished()) {
                subWinners[r][c] = IA_AND_PLAYER;
            } else {
                subWinners[r][c] = w;
            }

            if (w == myColor) {
                score += 65 * posBonus[r][c];
            } else if (w == opponentColor) {
                score -= 65 * posBonus[r][c];
            } else if (!sb.isFinished()) {
                // Sous-grille en cours : on regarde aussi son etat interne
                score += evaluateSubBoard(sb, myColor);
                score -= evaluateSubBoard(sb, opponentColor);
            }
            // (sous-grille nulle : contribution nette nulle, deja correct)
        }
    }

    // --- (2) Menaces 2-en-ligne au niveau global ----------------------------
    // Asymetrie volontaire : on penalise un peu plus les menaces adverses
    // pour pousser la defense quand elle est necessaire.
    int myThreats  = countTwoInLine(subWinners, myColor);
    int oppThreats = countTwoInLine(subWinners, opponentColor);
    score += 140 * myThreats;
    score -= 180 * oppThreats;

    // --- (3) Forks au niveau global -----------------------------------------
    // Avoir 2+ menaces simultanees = l'adversaire ne peut bloquer qu'une seule
    // -> quasi-mat. Tres gros poids. Asymetrie marquee.
    if (myThreats  >= 2) score += 800;
    if (oppThreats >= 2) score -= 1000;

    // --- (4) Lignes ouvertes (gradient meme sans menaces immediates) --------
    int myOpen  = countOpenLines(subWinners, myColor);
    int oppOpen = countOpenLines(subWinners, opponentColor);
    score += 25 * (myOpen - oppOpen);

    // --- (5) Facteur "trait" : ou doit jouer le side-to-move ? --------------
    int targetR = board.getTargetRow();
    int targetC = board.getTargetCol();
    int sign    = meToMove ? 1 : -1; // un avantage du side-to-move est positif si c'est nous

    if (targetR == -1) {
        // Choix libre : avantage de mobilite
        score += sign * 60;
    } else {
        const SubBoard& target = board.getSubBoard(targetR, targetC);
        if (!target.isFinished()) {
            Winner self  = meToMove ? myColor : opponentColor;
            Winner other = meToMove ? opponentColor : myColor;
            if (target.hasOneMoveWin(self)) {
                // Le joueur au trait peut gagner cette sous-grille tout de suite
                score += sign * 90;
            }
            if (target.hasOneMoveWin(other)) {
                // L'autre y a une menace -> le trait doit defendre
                score -= sign * 50;
            }
        }
    }

    return score;
}

// Eval interne d'une sous-grille non terminee. Encourage les positions
// fortes (centre, coins) et les menaces (2-en-ligne, forks locaux).
int MinimaxAI::evaluateSubBoard(const SubBoard& sb, Winner who) const {
    Winner cells[3][3];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            cells[r][c] = sb.getCell(r, c);

    int score = 0;
    if (cells[1][1] == who) score += 3; // centre
    if (cells[0][0] == who) score += 2; // 4 coins
    if (cells[0][2] == who) score += 2;
    if (cells[2][0] == who) score += 2;
    if (cells[2][2] == who) score += 2;

    int threats = countTwoInLine(cells, who);
    score += 5 * threats;
    // Fork sous-grille : 2+ menaces simultanees = quasi-victoire locale
    // (l'adversaire ne peut bloquer qu'une menace par coup).
    if (threats >= 2) score += 40;

    return score;
}

// Compte les lignes globales encore gagnables pour `who` : aucune cellule
// ne doit etre adverse, ni une nulle (encodee IA_AND_PLAYER).
// Iteration via SubBoard::LINES (precalculee, partagee).
int MinimaxAI::countOpenLines(const Winner sw[3][3], Winner who) const {
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        bool open = true;
        for (int k = 0; k < 3; ++k) {
            int r = SubBoard::LINES[i][k][0];
            int c = SubBoard::LINES[i][k][1];
            Winner x = sw[r][c];
            if (x != who && x != NO_WINNER) { open = false; break; }
        }
        if (open) ++count;
    }
    return count;
}

// Compte les menaces "2-en-ligne" pour `who` : exactement 2 cases occupees
// par who, 1 case vide, et aucune case adverse / nulle (bloqueur).
int MinimaxAI::countTwoInLine(const Winner cells[3][3], Winner who) const {
    int total = 0;
    for (int i = 0; i < 8; ++i) {
        int  mine    = 0;
        bool blocked = false;
        for (int k = 0; k < 3; ++k) {
            int r = SubBoard::LINES[i][k][0];
            int c = SubBoard::LINES[i][k][1];
            Winner x = cells[r][c];
            if (x == who)            mine++;
            else if (x != NO_WINNER) blocked = true; // adversaire ou nulle
        }
        if (mine == 2 && !blocked) total++;
    }
    return total;
}
