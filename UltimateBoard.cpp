#include "UltimateBoard.h"
#include <random>

uint64_t UltimateBoard::zCells[81][2];
uint64_t UltimateBoard::zTarget[10];
uint64_t UltimateBoard::zSide;
bool UltimateBoard::zInitialized = false;

void UltimateBoard::initZobrist() {
    // Seed fixe pour que le hash soit deterministe entre executions
    std::mt19937_64 rng(0xC0FFEE12345ULL);
    for (int i = 0; i < 81; ++i)
        for (int p = 0; p < 2; ++p)
            zCells[i][p] = rng();
    for (int i = 0; i < 10; ++i)
        zTarget[i] = rng();
    zSide = rng();
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

    updateOverallWinner();
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
    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            GameMove m{r, c};
            if (isValidMove(m)) out[n++] = m;
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

void UltimateBoard::updateOverallWinner() {
    Winner w[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            w[i][j] = subBoards[i][j].getWinner();

    for (int i = 0; i < 3; ++i) {
        if (w[i][0] != NO_WINNER && w[i][0] == w[i][1] && w[i][1] == w[i][2]) {
            overallWinner = w[i][0];
            return;
        }
        if (w[0][i] != NO_WINNER && w[0][i] == w[1][i] && w[1][i] == w[2][i]) {
            overallWinner = w[0][i];
            return;
        }
    }
    if (w[0][0] != NO_WINNER && w[0][0] == w[1][1] && w[1][1] == w[2][2]) {
        overallWinner = w[0][0];
        return;
    }
    if (w[0][2] != NO_WINNER && w[0][2] == w[1][1] && w[1][1] == w[2][0]) {
        overallWinner = w[0][2];
        return;
    }

    overallWinner = NO_WINNER;
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
