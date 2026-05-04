#include <iostream>
#include "main.h"
#include <cstdlib>
#include "UltimateGameManager.h"

int main() {
    // On instancie notre chef d'orchestre
    UltimateGameManager manager;

    // On lance le tournoi !
    // Paramètres : 10 parties, Niveau EASY_1, Mode DEBUG, On ne force pas le 1er tour, Pseudo.
    manager.playTournament(30, Level::HARD_2, Mode::ARENA, false, "Geraud");

    return 0;
}
