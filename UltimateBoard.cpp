#include "UltimateBoard.h"

uint64_t UltimateBoard::zCells[81][2];
uint64_t UltimateBoard::zTarget[10];
uint64_t UltimateBoard::zSide;
bool UltimateBoard::zInitialized = false;

// xorshift64 : generateur pseudo-aleatoire ultra-simple.
// Alternative a std::mt19937_64 qui evite d'inclure <random>.
// Le seed est fixe pour avoir un hash Zobrist deterministe entre executions.
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

UltimateBoard::UltimateBoard()
    : targetSubRow(-1), targetSubCol(-1), overallWinner(NO_WINNER),
      historySize(0), zobristHash(0) {
    if (!zInitialized) initZobrist();
    // Etat initial : choix libre (index 9), aucune cellule, mon trait (zSide non XOR)
    zobristHash = zTarget[9];
}

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

void UltimateBoard::playMove(GameMove m, Winner player) {
    int sR = m.row / 3;
    int sC = m.col / 3;
    int lR = m.row % 3;
    int lC = m.col % 3;

    UndoInfo& info = history[historySize++];
    info.move = m;
    info.prevTargetRow = targetSubRow;
    info.prevTargetCol = targetSubCol;
    info.prevOverallWinner = overallWinner;
    info.prevSubBoardWinner = subBoards[sR][sC].getWinner();
    info.prevHash = zobristHash;

    // Defaire l'ancien target dans le hash
    int oldTargetIdx = (targetSubRow == -1) ? 9 : (targetSubRow * 3 + targetSubCol);
    zobristHash ^= zTarget[oldTargetIdx];

    // Jouer + mettre a jour la cellule dans le hash
    subBoards[sR][sC].playMove(lR, lC, player);
    int cellIdx = m.row * 9 + m.col;
    int playerIdx = (player == IA) ? 0 : 1;
    zobristHash ^= zCells[cellIdx][playerIdx];
    zobristHash ^= zSide;

    // Determiner le nouveau target
    if (subBoards[lR][lC].isFinished()) {
        targetSubRow = -1;
        targetSubCol = -1;
    } else {
        targetSubRow = lR;
        targetSubCol = lC;
    }

    // Encoder le nouveau target dans le hash
    int newTargetIdx = (targetSubRow == -1) ? 9 : (targetSubRow * 3 + targetSubCol);
    zobristHash ^= zTarget[newTargetIdx];

    updateOverallWinner(sR, sC);
}

void UltimateBoard::undoMove() {
    if (historySize == 0) return;

    UndoInfo& info = history[--historySize];

    int sR = info.move.row / 3;
    int sC = info.move.col / 3;
    int lR = info.move.row % 3;
    int lC = info.move.col % 3;

    subBoards[sR][sC].undoMove(lR, lC);

    targetSubRow = info.prevTargetRow;
    targetSubCol = info.prevTargetCol;
    overallWinner = info.prevOverallWinner;
    zobristHash = info.prevHash;
}

int UltimateBoard::fillLegalMoves(GameMove out[81]) const {
    int n = 0;

    if (targetSubRow == -1) {
        // Choix libre : on parcourt toutes les sous-grilles non finies
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
        // C'est le cas le plus frequent en UTTT, et on saute 72 cellules.
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

std::vector<GameMove> UltimateBoard::getLegalMoves() const {
    std::vector<GameMove> moves;
    moves.reserve(20);
    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            GameMove m{r, c};
            if (isValidMove(m)) moves.push_back(m);
        }
    }
    return moves;
}

void UltimateBoard::updateOverallWinner(int sR, int sC) {
    // Apres playMove, seule subBoards[sR][sC] a pu changer d'etat.
    // Si elle n'a pas de gagnant, aucune ligne globale ne peut s'etre completee.
    Winner who = subBoards[sR][sC].getWinner();
    if (who == NO_WINNER) return;

    // On ne verifie QUE les lignes qui passent par (sR, sC) :
    // - sa rangee (toujours)
    // - sa colonne (toujours)
    // - la diagonale principale (si sR == sC)
    // - l'anti-diagonale (si sR + sC == 2)

    // Rangee sR
    if (subBoards[sR][0].getWinner() == who &&
        subBoards[sR][1].getWinner() == who &&
        subBoards[sR][2].getWinner() == who) {
        overallWinner = who;
        return;
    }
    // Colonne sC
    if (subBoards[0][sC].getWinner() == who &&
        subBoards[1][sC].getWinner() == who &&
        subBoards[2][sC].getWinner() == who) {
        overallWinner = who;
        return;
    }
    // Diagonale principale
    if (sR == sC &&
        subBoards[0][0].getWinner() == who &&
        subBoards[1][1].getWinner() == who &&
        subBoards[2][2].getWinner() == who) {
        overallWinner = who;
        return;
    }
    // Anti-diagonale
    if (sR + sC == 2 &&
        subBoards[0][2].getWinner() == who &&
        subBoards[1][1].getWinner() == who &&
        subBoards[2][0].getWinner() == who) {
        overallWinner = who;
        return;
    }
}

int UltimateBoard::countSubBoardsWon(Winner w) const {
    int n = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (subBoards[i][j].getWinner() == w) ++n;
    return n;
}

bool UltimateBoard::isTerminal() const {
    if (overallWinner != NO_WINNER) return true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!subBoards[i][j].isFinished()) return false;
    return true;
}
