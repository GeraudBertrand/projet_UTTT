#include "SubBoard.h"

// ============================================================
//          Tables de lignes 3x3 precalculees
// ============================================================

// Les 8 lignes du tic-tac-toe : rangees, colonnes, diagonales.
const int SubBoard::LINES[8][3][2] = {
    { {0,0}, {0,1}, {0,2} }, // rangee 0
    { {1,0}, {1,1}, {1,2} }, // rangee 1
    { {2,0}, {2,1}, {2,2} }, // rangee 2
    { {0,0}, {1,0}, {2,0} }, // colonne 0
    { {0,1}, {1,1}, {2,1} }, // colonne 1
    { {0,2}, {1,2}, {2,2} }, // colonne 2
    { {0,0}, {1,1}, {2,2} }, // diagonale principale
    { {0,2}, {1,1}, {2,0} }, // anti-diagonale
};

// Pour chaque case (r, c), les 2 AUTRES cases de chaque ligne qui passe par elle.
// {r1, c1, r2, c2}. r1 == -1 marque la fin.
// Cases coin : 3 lignes (rangee + colonne + 1 diag).
// Cases bord : 2 lignes (rangee + colonne).
// Centre (1,1) : 4 lignes (rangee + colonne + 2 diags).
const int SubBoard::LINE_PAIRS[3][3][4][4] = {
    // r = 0
    {
        // (0,0) : rangee, colonne, diag principale
        { {0,1, 0,2}, {1,0, 2,0}, {1,1, 2,2}, {-1,-1,-1,-1} },
        // (0,1) : rangee, colonne
        { {0,0, 0,2}, {1,1, 2,1}, {-1,-1,-1,-1}, {-1,-1,-1,-1} },
        // (0,2) : rangee, colonne, anti-diag
        { {0,0, 0,1}, {1,2, 2,2}, {1,1, 2,0}, {-1,-1,-1,-1} },
    },
    // r = 1
    {
        // (1,0) : rangee, colonne
        { {1,1, 1,2}, {0,0, 2,0}, {-1,-1,-1,-1}, {-1,-1,-1,-1} },
        // (1,1) : rangee, colonne, diag, anti-diag
        { {1,0, 1,2}, {0,1, 2,1}, {0,0, 2,2}, {0,2, 2,0} },
        // (1,2) : rangee, colonne
        { {1,0, 1,1}, {0,2, 2,2}, {-1,-1,-1,-1}, {-1,-1,-1,-1} },
    },
    // r = 2
    {
        // (2,0) : rangee, colonne, anti-diag
        { {2,1, 2,2}, {0,0, 1,0}, {0,2, 1,1}, {-1,-1,-1,-1} },
        // (2,1) : rangee, colonne
        { {2,0, 2,2}, {0,1, 1,1}, {-1,-1,-1,-1}, {-1,-1,-1,-1} },
        // (2,2) : rangee, colonne, diag principale
        { {2,0, 2,1}, {0,2, 1,2}, {0,0, 1,1}, {-1,-1,-1,-1} },
    },
};

// ============================================================
//                   SubBoard
// ============================================================

SubBoard::SubBoard() : boardWinner(NO_WINNER), filledCellsCount(0) {
    for(int i=0; i<3; ++i)
        for(int j=0; j<3; ++j)
            cells[i][j] = NO_WINNER;
}

void SubBoard::playMove(int r, int c, Winner player) {
    cells[r][c] = player;
    filledCellsCount++;
    checkWinCondition(r, c, player);
}

void SubBoard::undoMove(int r, int c) {
    cells[r][c] = NO_WINNER;
    filledCellsCount--;
    boardWinner = NO_WINNER;
}

void SubBoard::checkWinCondition(int r, int c, Winner player) {
    // On verifie uniquement les lignes qui passent par (r, c) via LINE_PAIRS.
    for (int i = 0; i < 4; ++i) {
        int r1 = LINE_PAIRS[r][c][i][0];
        if (r1 < 0) break; // plus de lignes pour cette case
        int c1 = LINE_PAIRS[r][c][i][1];
        int r2 = LINE_PAIRS[r][c][i][2];
        int c2 = LINE_PAIRS[r][c][i][3];
        if (cells[r1][c1] == player && cells[r2][c2] == player) {
            boardWinner = player;
            return;
        }
    }
}

bool SubBoard::wouldCompleteLine(int r, int c, Winner player) const {
    if (cells[r][c] != NO_WINNER) return false;
    for (int i = 0; i < 4; ++i) {
        int r1 = LINE_PAIRS[r][c][i][0];
        if (r1 < 0) break;
        int c1 = LINE_PAIRS[r][c][i][1];
        int r2 = LINE_PAIRS[r][c][i][2];
        int c2 = LINE_PAIRS[r][c][i][3];
        if (cells[r1][c1] == player && cells[r2][c2] == player) return true;
    }
    return false;
}

bool SubBoard::hasOneMoveWin(Winner player) const {
    if (isFinished()) return false;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (cells[r][c] != NO_WINNER) continue;
            // wouldCompleteLine inline (on a deja verifie que la case est vide)
            for (int i = 0; i < 4; ++i) {
                int r1 = LINE_PAIRS[r][c][i][0];
                if (r1 < 0) break;
                int c1 = LINE_PAIRS[r][c][i][1];
                int r2 = LINE_PAIRS[r][c][i][2];
                int c2 = LINE_PAIRS[r][c][i][3];
                if (cells[r1][c1] == player && cells[r2][c2] == player) return true;
            }
        }
    }
    return false;
}
