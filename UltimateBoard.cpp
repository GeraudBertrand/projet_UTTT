#include "UltimateBoard.h"

// =============================================================================
//  Tables Zobrist (statiques) et leur initialisation
// =============================================================================

uint64_t UltimateBoard::zCells[81][2];
uint64_t UltimateBoard::zTarget[10];
uint64_t UltimateBoard::zSide;
bool     UltimateBoard::zInitialized = false;

// xorshift64 : generateur pseudo-aleatoire ultra-simple.
// On ne veut PAS de qualite cryptographique, juste une distribution decente
// pour faire des hash Zobrist. Avantage par rapport a std::mt19937_64 :
// pas besoin d'inclure <random> (4 lignes au lieu d'une bibliotheque).
// Seed fixe -> les hashes Zobrist sont deterministes entre executions.
static uint64_t xs_state = 0xC0FFEE12345ULL;
static uint64_t xorshift64() {
    xs_state ^= xs_state << 13;
    xs_state ^= xs_state >> 7;
    xs_state ^= xs_state << 17;
    return xs_state;
}

void UltimateBoard::initZobrist() {
    xs_state = 0xC0FFEE12345ULL; // reset au cas ou
    for (int i = 0; i < 81; ++i)
        for (int p = 0; p < 2; ++p)
            zCells[i][p] = xorshift64();
    for (int i = 0; i < 10; ++i)
        zTarget[i] = xorshift64();
    zSide = xorshift64();
    zInitialized = true;
}

// =============================================================================
//  Cycle de vie
// =============================================================================

UltimateBoard::UltimateBoard()
    : targetSubRow(-1), targetSubCol(-1), overallWinner(NO_WINNER),
      historySize(0), zobristHash(0) {
    if (!zInitialized) initZobrist();
    // Etat initial du hash : on encode la cible "choix libre" (index 9).
    // Le bit de side-to-move (zSide) est implicitement a 0.
    zobristHash = zTarget[9];
}

// =============================================================================
//  Regles de jeu
// =============================================================================

// Verifie les 3 regles UTTT :
//   1) la case est dans le plateau
//   2) la case est vide et sa sous-grille n'est pas terminee
//   3) elle est dans la sous-grille forcee (sauf si target == -1 = choix libre)
bool UltimateBoard::isValidMove(GameMove m) const {
    if (m.row < 0 || m.row > 8 || m.col < 0 || m.col > 8) return false;

    int sR = m.row / 3;
    int sC = m.col / 3;
    int lR = m.row % 3;
    int lC = m.col % 3;

    if (subBoards[sR][sC].getCell(lR, lC) != NO_WINNER) return false;
    if (subBoards[sR][sC].isFinished()) return false;
    if (targetSubRow != -1 && (sR != targetSubRow || sC != targetSubCol)) return false;

    return true;
}

// Joue un coup et met a jour :
//   - la sous-grille concernee
//   - la cible imposee a l'adversaire (= la position locale du coup joue)
//   - le hash Zobrist (incrementalement)
//   - le gagnant global
// L'etat avant le coup est sauvegarde dans la pile pour pouvoir defaire.
void UltimateBoard::playMove(GameMove m, Winner player) {
    int sR = m.row / 3;
    int sC = m.col / 3;
    int lR = m.row % 3;
    int lC = m.col % 3;

    // 1) On sauvegarde tout ce qu'il faut pour annuler ce coup.
    UndoInfo& info = history[historySize++];
    info.move              = m;
    info.prevTargetRow     = targetSubRow;
    info.prevTargetCol     = targetSubCol;
    info.prevOverallWinner = overallWinner;
    info.prevSubBoardWinner = subBoards[sR][sC].getWinner();
    info.prevHash          = zobristHash;

    // 2) Hash : on retire l'ancienne cible.
    int oldTargetIdx = (targetSubRow == -1) ? 9 : (targetSubRow * 3 + targetSubCol);
    zobristHash ^= zTarget[oldTargetIdx];

    // 3) On joue dans la sous-grille + on ajoute la cellule au hash.
    subBoards[sR][sC].playMove(lR, lC, player);
    int cellIdx   = m.row * 9 + m.col;
    int playerIdx = (player == IA) ? 0 : 1;
    zobristHash ^= zCells[cellIdx][playerIdx];
    zobristHash ^= zSide; // le trait change

    // 4) Nouvelle cible imposee : la position locale du coup joue.
    //    Sauf si cette sous-grille est deja finie -> choix libre.
    if (subBoards[lR][lC].isFinished()) {
        targetSubRow = -1;
        targetSubCol = -1;
    } else {
        targetSubRow = lR;
        targetSubCol = lC;
    }

    // 5) Hash : on ajoute la nouvelle cible.
    int newTargetIdx = (targetSubRow == -1) ? 9 : (targetSubRow * 3 + targetSubCol);
    zobristHash ^= zTarget[newTargetIdx];

    // 6) Verifier si on vient de remporter le plateau global.
    updateOverallWinner(sR, sC);
}

// Restaure exactement l'etat sauvegarde au playMove correspondant.
// Note : on restore le hash directement (plus simple que de re-XOR).
void UltimateBoard::undoMove() {
    if (historySize == 0) return;
    UndoInfo& info = history[--historySize];

    int sR = info.move.row / 3;
    int sC = info.move.col / 3;
    int lR = info.move.row % 3;
    int lC = info.move.col % 3;

    subBoards[sR][sC].undoMove(lR, lC);

    targetSubRow  = info.prevTargetRow;
    targetSubCol  = info.prevTargetCol;
    overallWinner = info.prevOverallWinner;
    zobristHash   = info.prevHash;
}

// =============================================================================
//  Generation des coups legaux
// =============================================================================

// Optimisation cle : si target != -1, on ne parcourt QUE les 9 cases de la
// sous-grille forcee. Sinon (choix libre), on parcourt les sous-grilles
// encore ouvertes. On evite ainsi le "for r=0..8, c=0..8" + isValidMove().
int UltimateBoard::fillLegalMoves(GameMove out[81]) const {
    int n = 0;

    if (targetSubRow == -1) {
        // Choix libre : parcourir toutes les sous-grilles non terminees.
        for (int sR = 0; sR < 3; ++sR) {
            for (int sC = 0; sC < 3; ++sC) {
                if (subBoards[sR][sC].isFinished()) continue;
                int rowBase = sR * 3;
                int colBase = sC * 3;
                for (int lR = 0; lR < 3; ++lR) {
                    for (int lC = 0; lC < 3; ++lC) {
                        if (subBoards[sR][sC].getCell(lR, lC) == NO_WINNER) {
                            out[n].row = rowBase + lR;
                            out[n].col = colBase + lC;
                            ++n;
                        }
                    }
                }
            }
        }
    } else {
        // Sous-grille forcee : on n'examine que ses 9 cellules.
        // C'est le cas le plus frequent en UTTT (on saute 72 cases).
        int sR = targetSubRow;
        int sC = targetSubCol;
        int rowBase = sR * 3;
        int colBase = sC * 3;
        for (int lR = 0; lR < 3; ++lR) {
            for (int lC = 0; lC < 3; ++lC) {
                if (subBoards[sR][sC].getCell(lR, lC) == NO_WINNER) {
                    out[n].row = rowBase + lR;
                    out[n].col = colBase + lC;
                    ++n;
                }
            }
        }
    }

    return n;
}

// =============================================================================
//  Mise a jour du gagnant global
// =============================================================================

// Apres playMove, seule subBoards[sR][sC] a pu changer d'etat.
// Donc seules 2 a 4 lignes globales (celles qui passent par (sR, sC)) peuvent
// avoir ete completees. On les enumere via SubBoard::LINE_PAIRS, partage avec
// la detection de victoire interne aux sous-grilles.
void UltimateBoard::updateOverallWinner(int sR, int sC) {
    Winner who = subBoards[sR][sC].getWinner();
    if (who == NO_WINNER) return; // pas de nouveau gagnant local, rien a faire

    for (int i = 0; i < 4; ++i) {
        int r1 = SubBoard::LINE_PAIRS[sR][sC][i][0];
        if (r1 < 0) break;
        int c1 = SubBoard::LINE_PAIRS[sR][sC][i][1];
        int r2 = SubBoard::LINE_PAIRS[sR][sC][i][2];
        int c2 = SubBoard::LINE_PAIRS[sR][sC][i][3];
        if (subBoards[r1][c1].getWinner() == who &&
            subBoards[r2][c2].getWinner() == who) {
            overallWinner = who;
            return;
        }
    }
}

// =============================================================================
//  Queries d'etat
// =============================================================================

int UltimateBoard::countSubBoardsWon(Winner w) const {
    int n = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (subBoards[i][j].getWinner() == w) ++n;
    return n;
}

// Le plateau est termine soit avec un gagnant global, soit quand toutes les
// sous-grilles sont elles-memes terminees (gagnees ou pleines).
bool UltimateBoard::isTerminal() const {
    if (overallWinner != NO_WINNER) return true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!subBoards[i][j].isFinished()) return false;
    return true;
}
