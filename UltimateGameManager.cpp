#include "UltimateGameManager.h"
#include <ctime>      // time, strftime, localtime (pour le nom du log)
#include <iostream>

// =============================================================================
//  Cycle de vie
// =============================================================================

UltimateGameManager::UltimateGameManager()
    : myColor(PLAYER), opponentColor(IA),
      // maxDepth = plafond ; la limite reelle vient du budget de temps.
      ai(PLAYER, IA, 5, 7500),
      gameNumber(0), moveIndex(0) {
    openLogFile();

    // Active le livre d'ouvertures :
    //   - alimente jusqu'au 12e coup joue (= ~6 plies de chaque cote) ;
    //   - profondeur supplementaire +4 sur ces positions (depth 12 au lieu de 8) ;
    //   - budget supplementaire +5000 ms pour avoir le temps d'aller chercher.
    // Premiere partie : lente sur les premiers coups (deep think + ecriture).
    // Parties suivantes : positions deja en cache => reponses instantanees.
    ai.enableBook("opening_book.txt", 12, 4, 5000);
}

UltimateGameManager::~UltimateGameManager() {
    if (logFile.is_open()) logFile.close();
}

// =============================================================================
//  Logger CSV
// =============================================================================
//
//  On ecrit un fichier par execution, nomme ai_log_YYYYMMDD_HHMMSS.csv,
//  avec une ligne par evenement : debut/fin de partie, coup adverse, coup IA.
//  Chaque ligne respecte le meme schema 14 colonnes pour etre charge facilement
//  dans pandas / Excel.
// =============================================================================

void UltimateGameManager::openLogFile() {
    // Nom de fichier avec timestamp pour ne pas ecraser les runs precedents.
    // strftime + localtime sont en C standard, simples et portables.
    time_t t = time(NULL);
    char filename[64];
    strftime(filename, sizeof(filename), "ai_log_%Y%m%d_%H%M%S.csv", localtime(&t));

    logFile.open(filename);
    if (!logFile.is_open()) {
        std::cerr << "[Logger] Impossible d'ouvrir " << filename << std::endl;
        return;
    }

    // En-tete CSV (14 colonnes).
    logFile << "event,game,move_idx,row,col,target_r_before,target_c_before,"
               "depth,score,elapsed_ms,legal_count,had_local_win,took_local_win,result"
            << std::endl;
    logFile.flush();
    std::cout << "[Logger] Ecriture vers " << filename << std::endl;
}

void UltimateGameManager::writeGameStartRow() {
    if (!logFile.is_open()) return;
    logFile << "game_start," << gameNumber << ",0,,,,,,,,,,," << std::endl;
    logFile.flush();
}

void UltimateGameManager::writeGameEndRow(Winner result) {
    if (!logFile.is_open()) return;
    const char* resStr = "draw";
    if      (result == myColor)        resStr = "win";
    else if (result == opponentColor)  resStr = "loss";
    else if (result == IA_AND_PLAYER)  resStr = "both";
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
    // Recupere les details de la derniere recherche pour le log.
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

// =============================================================================
//  Boucle principale du tournoi
// =============================================================================
//
//  Une partie se deroule comme suit :
//   1) On attend (bloquant) le coup adverse via game.getMove().
//   2) S'il est valide, on le rejoue sur notre plateau interne.
//   3) On calcule notre coup via l'IA, on le joue, on l'envoie au moteur.
//   4) On repete jusqu'a game.isFinish().
//
//  Entre chaque partie, on reinitialise le plateau interne et on sauve
//  le livre d'ouvertures (au cas ou le programme crash plus tard).
// =============================================================================
void UltimateGameManager::playTournament(int nbGames, Level level, Mode mode,
                                          bool alwaysPlayFirst,
                                          const std::string& alias) {
    game.initialize(nbGames, level, mode, alwaysPlayFirst, alias);

    while (!game.isAllGameFinish()) {
        ++gameNumber;
        moveIndex = 0;

        resetForNewGame();
        writeGameStartRow();

        while (!game.isFinish()) {
            // --- (1) Lire le coup adverse ----------------------------------
            GameMove opponentMove;
            game.getMove(opponentMove);

            // Au tout premier coup quand on commence la partie, le moteur peut
            // remplir opponentMove avec (-1, -1). On filtre ce cas.
            if (opponentMove.row >= 0 && opponentMove.col >= 0
                && opponentMove.row < 9 && opponentMove.col < 9) {
                int tR = internalBoard.getTargetRow();
                int tC = internalBoard.getTargetCol();
                internalBoard.playMove(opponentMove, opponentColor);
                ++moveIndex;
                writeOppMoveRow(opponentMove, tR, tC);
            }

            // --- (2) Capturer le target AVANT notre coup (pour le log) -----
            int tR = internalBoard.getTargetRow();
            int tC = internalBoard.getTargetCol();

            // --- (3) Notre coup --------------------------------------------
            GameMove myMove = calculateBestMove();
            internalBoard.playMove(myMove, myColor);
            ++moveIndex;
            writeAiMoveRow(myMove, tR, tC);

            // --- (4) Envoyer au moteur du prof ----------------------------
            game.setMove(myMove);
        }

        // Fin de partie : on log le resultat et on sauve le livre.
        writeGameEndRow(game.getWinner());
        ai.saveBook();
    }
}

// =============================================================================
//  Helpers internes
// =============================================================================

void UltimateGameManager::resetForNewGame() {
    internalBoard = UltimateBoard();
}

GameMove UltimateGameManager::calculateBestMove() {
    return ai.findBestMove(internalBoard);
}
