#ifndef ULTIMATEGAMEMANAGER_H
#define ULTIMATEGAMEMANAGER_H

#include "UltimateBoard.h"
#include "MinimaxAI.h"
#include "main.h"
#include <fstream>
#include <string>

class UltimateGameManager {
private:
    UltimateBoard internalBoard;
    Winner myColor;
    Winner opponentColor;
    MinimaxAI ai;

    // Logger CSV
    std::ofstream logFile;
    int gameNumber;
    int moveIndex;

public:
    UltimateGameManager();
    ~UltimateGameManager();

    void playTournament(int nbGames, Level level, Mode mode, bool alwaysPlayFirst, const std::string& alias);

private:
    void resetForNewGame();
    GameMove calculateBestMove();

    void openLogFile();
    void writeAiMoveRow(GameMove m, int targetRBefore, int targetCBefore);
    void writeOppMoveRow(GameMove m, int targetRBefore, int targetCBefore);
    void writeGameStartRow();
    void writeGameEndRow(Winner result);
};

#endif
