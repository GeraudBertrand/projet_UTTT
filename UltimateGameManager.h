#ifndef ULTIMATEGAMEMANAGER_H
#define ULTIMATEGAMEMANAGER_H

#include "UltimateBoard.h"
#include "MinimaxAI.h"
#include "main.h"
#include <fstream>
#include <string>

// =============================================================================
//  UltimateGameManager : chef d'orchestre du tournoi.
//
//  Responsabilites :
//   - dialogue avec le moteur du prof (interface `game` declaree dans main.h)
//   - maintient notre vision du plateau (internalBoard)
//   - delegue le choix du coup a MinimaxAI
//   - enregistre un log CSV des decisions pour analyser les parties
// =============================================================================
class UltimateGameManager {
private:
    // -------- Etat du jeu --------
    UltimateBoard internalBoard;
    Winner        myColor;
    Winner        opponentColor;
    MinimaxAI     ai;

    // -------- Logger CSV --------
    std::ofstream logFile;
    int           gameNumber;
    int           moveIndex;

public:
    UltimateGameManager();
    ~UltimateGameManager();

    // Lance le tournoi complet : ouvre le moteur, joue toutes les parties.
    void playTournament(int nbGames, Level level, Mode mode,
                        bool alwaysPlayFirst, const std::string& alias);

private:
    void resetForNewGame();
    GameMove calculateBestMove();

    // -------- Logger : ouverture + ecriture d'une ligne par evenement --------
    void openLogFile();
    void writeGameStartRow();
    void writeGameEndRow(Winner result);
    void writeOppMoveRow(GameMove m, int targetRBefore, int targetCBefore);
    void writeAiMoveRow(GameMove m, int targetRBefore, int targetCBefore);
};

#endif
