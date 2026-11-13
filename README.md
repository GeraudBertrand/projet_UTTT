# Projet UTTT — IA pour Ultimate Tic-Tac-Toe

Projet de C++ : IA qui joue à l'Ultimate Tic-Tac-Toe contre le moteur fourni
par le prof. L'algo principal est un **minimax avec alpha-beta**, plus
quelques optimisations classiques que j'ai ajoutées au fur et à mesure
(iterative deepening, transposition table, livre d'ouvertures…).

## Les règles du jeu (rappel)

- Plateau = 3×3 de mini-grilles 3×3 (donc 9×9 cases au total).
- Quand on joue dans une case locale `(r, c)`, **l'adversaire doit jouer
  dans la mini-grille `(r, c)`** au tour suivant.
- Si cette mini-grille est déjà gagnée ou pleine → choix libre.
- Gagner une mini-grille = aligner 3 pions dedans.
- Gagner le jeu = aligner 3 mini-grilles gagnées.
- Plateau plein sans gagnant → on compte les mini-grilles gagnées.

## Les fichiers

```
main.cpp / main.h            entry point + types partages (Winner, GameMove, etc.)
subBoard.{h,cpp}             une mini-grille 3x3
UltimateBoard.{h,cpp}        le plateau complet + regles UTTT + hash Zobrist
OpeningBook.{h,cpp}          livre d'ouvertures (positions deja analysees)
MinimaxAI.{h,cpp}            l'IA
UltimateGameManager.{h,cpp}  pilote le tournoi et log les parties

libUTTTLib.a + *.dll         moteur du prof + Allegro (necessaires au build)
images/                      sprites du mode graphique
opening_book.txt             livre d'ouvertures (genere par l'IA)
bin/Debug/UTTT_Template.exe  dernier binaire compile
docs/                        diagramme de classes (Mermaid + PNG)
logs/                        anciens logs CSV des tournois de test
```

Le diagramme de classes est dans [docs/class_diagram.md](docs/class_diagram.md)
(rendu par GitHub) et en image dans [docs/class_diagram.png](docs/class_diagram.png).

Chaque classe gère son truc :

- `SubBoard` sait poser un pion et détecter une victoire dans une mini-grille.
- `UltimateBoard` connaît les règles d'UTTT (target imposé, free choice).
  Il maintient aussi un hash Zobrist pour l'IA.
- `MinimaxAI` fait la recherche du meilleur coup.
- `UltimateGameManager` parle au moteur du prof et appelle l'IA.

## Compiler et lancer

Le projet utilise **Code::Blocks + MinGW** (le `.cbp` est inclus) et la
bibliothèque **Allegro 5.2** (les `.dll` sont à côté de l'exe).

1. Ouvrir `UTTT_Template.cbp` dans Code::Blocks.
2. Build → choisir Debug ou Release.
3. Run.

Pour tester en Debug c'est plus pratique (mode graphique), pour mesurer
les vrais perfs faut compiler en Release (`-O3`).

## Configurer le tournoi

Tout se passe dans `main.cpp` :

```cpp
manager.playTournament(
    100,                  // nombre de parties
    Level::MEDIUM_2,      // niveau adverse
    Mode::ARENA,          // ARENA ou DEBUG
    false,                // si true, on commence toujours
    "MonIA_C++"           // pseudo
);
```

Et les réglages de l'IA dans `UltimateGameManager.cpp` :

```cpp
ai(PLAYER, IA, 5, 7500);                    // depth max 5, budget 7500ms
ai.enableBook("opening_book.txt", 12, 4, 5000);
```

Si les parties sont trop longues, baisser le budget (genre `2000` ms).
Si ça gagne pas assez, monter (mais ça prend du temps).

## Ce que fait l'IA

Le `MinimaxAI` enchaîne plusieurs trucs :

- **Minimax + alpha-beta** : l'algo de base, coupe les branches inutiles.
- **Iterative deepening** : on cherche à profondeur 1, 2, 3… jusqu'au
  timeout. Comme ça on a toujours un coup à jouer même si on coupe.
- **Transposition table** : on cache les positions déjà évaluées (~256K
  entrées indexées par hash Zobrist). Évite de tout recalculer.
- **Move ordering** : on essaie les coups prometteurs en premier
  (gagne une mini-grille, coup TT, killer moves, history).
- **Opening book** : pour les positions de début de partie déjà analysées
  en profondeur, on a la réponse instantanée dans un fichier.
- **Eval** : pondère les mini-grilles gagnées (centre vaut plus que les
  coins), compte les menaces 2-en-ligne, détecte les forks…

## Le log CSV

À chaque exécution un fichier `ai_log_YYYYMMDD_HHMMSS.csv` est généré à la
racine (ignoré par git ; les logs utiles sont rangés à la main dans `logs/`).
Une ligne par évènement (`game_start`, `opp_move`, `ai_move`, `game_end`).

Colonnes :
```
event, game, move_idx, row, col, target_r_before, target_c_before,
depth, score, elapsed_ms, legal_count, had_local_win, took_local_win, result
```

C'est utile pour debugger : si tu vois que l'IA pouvait gagner une
mini-grille (`had_local_win=1`) mais l'a pas pris (`took_local_win=0`),
regarde le score. Si le score est positif elle a sacrifié intelligemment,
si il est négatif elle pense déjà avoir perdu.

## Le livre d'ouvertures

Le fichier `opening_book.txt` se remplit tout seul pendant les premières
parties. Format :

```
# OpeningBook : <hash hex> <row> <col>
a98c9f4b5758f7c6 4 0
1f95325c61319a1e 2 4
...
```

La première fois c'est lent sur les premiers coups (l'IA cherche profond
pour remplir le livre). Après les parties suivantes ces positions sont en
cache → réponse immédiate. Au bout de ~50 parties les ouvertures classiques
sont quasi toutes connues.

Si on change l'eval ou les poids, il vaut mieux supprimer le fichier pour
repartir d'un livre propre (sinon on garde des coups optimisés pour
l'ancienne version).

## Performance

Avec les réglages actuels, contre `MEDIUM_2` je suis autour de **72% de
victoires** sur 100 parties. Contre les niveaux plus faibles c'est plus.
Contre HARD_X je n'ai pas vraiment testé en profondeur.

## Limites

- **Debug vs Release** : en Debug c'est plusieurs fois plus lent, donc
  les chiffres de perf doivent être pris en Release.
- **Pas de quiescence search** : sur certaines positions tactiques l'IA
  voit le piège un coup trop tard (effet d'horizon).
- **Single thread** : un seul cœur utilisé.
