#include "UltimateGameManager.h"
#include <iostream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>

UltimateGameManager::UltimateGameManager()
    : myColor(PLAYER), opponentColor(IA),
      ai(PLAYER, IA, 15, 10000),
      gameNumber(0), moveIndex(0) {
    openLogFile();

    // Active le livre d'ouvertures :
    //  - alimente jusqu'au 12e coup joue (= ~6 plies de chaque cote)
    //  - profondeur supplementaire +4 sur ces positions (depth 14 au lieu de 10)
    //  - budget supplementaire +5000 ms pour avoir le temps d'aller chercher
    // Premiere partie : lente sur les premiers coups (deep think + ecriture).
    // Suivantes : positions deja en cache => reponses instantanees.
    ai.enableBook("opening_book.txt", 12, 4, 5000);
}

UltimateGameManager::~UltimateGameManager() {
    if (logFile.is_open()) logFile.close();
}

void UltimateGameManager::openLogFile() {
    // Nom de fichier avec timestamp pour ne pas ecraser les runs precedents
    std::time_t t = std::time(nullptr);
    std::tm tm_local;
#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    tm_local = *std::localtime(&t);
#endif
    std::ostringstream name;
    name << "ai_log_"
         << std::put_time(&tm_local, "%Y%m%d_%H%M%S")
         << ".csv";

    logFile.open(name.str());
    if (!logFile.is_open()) {
        std::cerr << "[Logger] Impossible d'ouvrir " << name.str() << std::endl;
        return;
    }

    // En-tete CSV
    logFile << "event,game,move_idx,row,col,target_r_before,target_c_before,"
               "depth,score,elapsed_ms,legal_count,had_local_win,took_local_win,result"
            << std::endl;
    logFile.flush();
    std::cout << "[Logger] Ecriture vers " << name.str() << std::endl;
}

void UltimateGameManager::writeGameStartRow() {
    if (!logFile.is_open()) return;
    logFile << "game_start," << gameNumber << ",0,,,,,,,,,,," << std::endl;
    logFile.flush();
}

void UltimateGameManager::writeGameEndRow(Winner result) {
    if (!logFile.is_open()) return;
    const char* resStr = "draw";
    if (result == myColor)        resStr = "win";
    else if (result == opponentColor) resStr = "loss";
    else if (result == IA_AND_PLAYER) resStr = "both";
    logFile << "game_end," << gameNumber << "," << moveIndex
            << ",,,,,,,,,,," << resStr << std::endl;
    logFile.flush();
}

void UltimateGameManager::writeOppMoveRow(GameMove m, int targetR, int targetC) {
    if (!logFile.is_open()) return;
    logFile << "opp_move," << gameNumber << "," << moveIndex << ","
            << m.row << "," << m.col << ","
            << targetR << "," << targetC
            << ",,,,,,," << std::endl;
}

void UltimateGameManager::writeAiMoveRow(GameMove m, int targetR, int targetC) {
    if (!logFile.is_open()) return;
    const MinimaxAI::SearchInfo& s = ai.getLastSearchInfo();
    logFile << "ai_move," << gameNumber << "," << moveIndex << ","
            << m.row << "," << m.col << ","
            << targetR << "," << targetC << ","
            << s.depthReached << ","
            << s.score << ","
            << s.elapsedMs << ","
            << s.legalMovesCount << ","
            << (s.hadLocalWin ? 1 : 0) << ","
            << (s.tookLocalWin ? 1 : 0) << ","
            << std::endl;
    logFile.flush();
}

void UltimateGameManager::playTournament(int nbGames, Level level, Mode mode, bool alwaysPlayFirst, const std::string& alias) {
    game.initialize(nbGames, level, mode, alwaysPlayFirst, alias);

    while (!game.isAllGameFinish()) {
        ++gameNumber;
        moveIndex = 0;

        resetForNewGame();
        writeGameStartRow();

        while (!game.isFinish()) {
            GameMove opponentMove;
            game.getMove(opponentMove);

            if (opponentMove.row >= 0 && opponentMove.col >= 0
                && opponentMove.row < 9 && opponentMove.col < 9) {
                int tR = internalBoard.getTargetRow();
                int tC = internalBoard.getTargetCol();
                internalBoard.playMove(opponentMove, opponentColor);
                ++moveIndex;
                writeOppMoveRow(opponentMove, tR, tC);
            }

            // Capture du target AVANT le coup IA pour le log
            int tR = internalBoard.getTargetRow();
            int tC = internalBoard.getTargetCol();

            GameMove myMove = calculateBestMove();
            internalBoard.playMove(myMove, myColor);
            ++moveIndex;
            writeAiMoveRow(myMove, tR, tC);

            game.setMove(myMove);
        }

        // Fin de partie
        writeGameEndRow(game.getWinner());
        // Sauve le livre apres chaque partie pour ne rien perdre en cas de crash
        ai.saveBook();
    }
}

void UltimateGameManager::resetForNewGame() {
    internalBoard = UltimateBoard();
}

GameMove UltimateGameManager::calculateBestMove() {
    return ai.findBestMove(internalBoard);
}
