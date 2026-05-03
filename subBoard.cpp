#include "SubBoard.h"

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
    boardWinner = NO_WINNER; // On simplifie : si on annule, on recalcule ou on reset
}

void SubBoard::checkWinCondition(int r, int c, Winner player) {
    // Vérification rapide de la ligne, colonne et diagonales
    // Ligne
    if (cells[r][0] == player && cells[r][1] == player && cells[r][2] == player) boardWinner = player;
    // Colonne
    else if (cells[0][c] == player && cells[1][c] == player && cells[2][c] == player) boardWinner = player;
    // Diagonales
    else if (r == c && cells[0][0] == player && cells[1][1] == player && cells[2][2] == player) boardWinner = player;
    else if (r + c == 2 && cells[0][2] == player && cells[1][1] == player && cells[2][0] == player) boardWinner = player;
}
