#ifndef SUBBOARD_H
#define SUBBOARD_H

#include "main.h" // Pour Winner

class SubBoard {
private:
    Winner cells[3][3];
    Winner boardWinner;
    int filledCellsCount;

public:
    SubBoard();

    // Jouer un coup (localRow/Col entre 0 et 2)
    void playMove(int r, int c, Winner player);
    void undoMove(int r, int c);

    // Accesseurs
    Winner getWinner() const { return boardWinner; }
    bool isFull() const { return filledCellsCount >= 9; }
    bool isFinished() const { return boardWinner != NO_WINNER || isFull(); }
    Winner getCell(int r, int c) const { return cells[r][c]; }

private:
    void checkWinCondition(int r, int c, Winner player);
};

#endif
