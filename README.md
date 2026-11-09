# minidb — un SGBD relationnel simplifié en C

minidb est une petite base de données relationnelle écrite en C11 (langage compilé, typage fort).
Elle ne comprend pas le SQL : on l'utilise par une API en C qui permet de :

- définir des tables dans un format simplifié ;
- insérer des lignes ;
- faire des requêtes (égalité, comparaisons, intervalles, plusieurs conditions combinées par ET) ;
- créer des index de trois types : table de hachage, arbre AVL, liste triée.

## Compiler et lancer

```sh
make            # compile demo, test et bench (gcc -std=c11 -O2 -Wall -Wextra)
./demo          # démonstration de l'API
./test          # tests de cohérence
./bench         # analyse de performance (environ 2 min)
./bench index        # uniquement la comparaison des index
./bench croissance   # uniquement la comparaison des stratégies de stockage
```

Sous Windows, utiliser `mingw32-make` si `make` n'est pas installé.

| Fichier | Rôle |
|---|---|
| `core/minidb.h` | API publique |
| `core/table.c` | tables, analyse des définitions, insertion, planificateur de requêtes |
| `index/index.h` | interface commune aux index |
| `index/index_hash.c` | index par table de hachage |
| `index/index_avl.c` | index par arbre AVL |
| `index/index_list.c` | index par liste chaînée triée (volontairement sous-optimal) |
| `core/util.c` | allocation, comparaison de valeurs, ensembles de résultats |
| `exemples/demo.c`, `tests/test.c`, `benchmark/bench.c` | démonstration, tests, benchmark |

## API

```c
Database *db = db_create();

/* Définition simplifiée : INT (entier 64 bits) ou TEXT(n) (n caractères max, 32 par défaut) */
Table *users = create_table_struct(db, "users(id INT, name TEXT(24), age INT)");

Value row[] = { VAL_INT(1), VAL_TEXT("Alice"), VAL_INT(31) };
insert_row(users, row, 3);

create_index(users, "id",  INDEX_HASH);   /* égalité */
create_index(users, "age", INDEX_AVL);    /* égalité et intervalles */

/* SELECT name FROM users WHERE age BETWEEN 25 AND 40 AND id > 3 */
Condition conds[] = {
    { "age", OP_BETWEEN, VAL_INT(25), VAL_INT(40) },
    { "id",  OP_GT,      VAL_INT(3),  {0} },
};
ResultSet rs = select_values(users, conds, 2);
const char *colonnes[] = { "name" };
print_rows(users, &rs, colonnes, 1);   /* affiche aussi le plan choisi */
result_free(&rs);

db_free(db);
```

Opérateurs disponibles : `OP_EQ`, `OP_LT`, `OP_LE`, `OP_GT`, `OP_GE`, `OP_BETWEEN`. Ils fonctionnent sur les deux types (ordre lexicographique pour `TEXT`).

Les définitions invalides (type inconnu, colonne en double, syntaxe incorrecte) et les insertions invalides (mauvais type, texte trop long, mauvais nombre de colonnes) sont refusées.

## Structures de données

### Stockage des lignes

Chaque table connaît son schéma, donc la taille d'une ligne est fixe : 8 octets par `INT`, n + 1 octets par `TEXT(n)`. Les lignes sont rangées les unes après les autres dans **un seul bloc mémoire**, et la position de chaque colonne dans la ligne est calculée une fois, à la création de la table. Résultats :

- l'accès à la ligne i et à sa colonne j se fait en O(1) ;
- il n'y a pas d'allocation par ligne ;
- un parcours complet lit la mémoire dans l'ordre, ce qui profite au cache du processeur.

L'identifiant d'une ligne est sa position dans ce bloc.

### Index

Un index associe chaque clé distincte à la **liste des lignes** qui ont cette valeur, ce qui gère les doublons. Les trois implémentations ont la même interface : insertion, recherche par égalité et recherche par intervalle.

- **Table de hachage** : chaînage séparé, 64 cases au départ. Le tableau double quand il y a autant de clés que de cases. Fonction de hachage : la valeur elle-même pour les entiers, h = h × 31 + c pour les chaînes, puis modulo le nombre de cases. L'ordre des clés est perdu, donc pas de recherche par intervalle.
- **Arbre AVL** : arbre binaire de recherche équilibré par rotations. Sa hauteur reste en O(log n) même si les clés arrivent triées, alors qu'un arbre binaire naïf dégénère en liste. Les intervalles sont traités par un parcours infixe qui ne visite que les sous-arbres utiles.
- **Liste chaînée triée** : implémentation **volontairement sous-optimale**, qui sert de point de comparaison. Il faut la parcourir pour insérer comme pour chercher, et ses nœuds sont dispersés en mémoire.

### Planificateur de requêtes

`select_values` choisit, parmi les conditions, celle qui a le meilleur index :

| | Hash | AVL | Liste |
|---|---|---|---|
| Égalité | 1er choix | 2e choix | 3e choix |
| Intervalle | inutilisable | 1er choix | 2e choix |

Il récupère les lignes candidates avec cet index, puis vérifie les autres conditions **seulement sur ces candidates**. S'il n'y a aucun index utilisable, il fait un parcours complet. `select_scan` force le parcours complet et sert de référence naïve. `test.c` vérifie sur des milliers de requêtes aléatoires que les deux fonctions donnent toujours les mêmes lignes.

## Complexité des opérations

Notations : n = nombre de lignes, k = nombre de lignes du résultat, c = nombre de colonnes, m = nombre de conditions.

| Opération | Sans index (naïf) | Liste triée | Table de hachage | Arbre AVL |
|---|---|---|---|---|
| `insert_row` (stockage) | O(c) amorti | O(c) amorti | O(c) amorti | O(c) amorti |
| Coût ajouté par l'index à chaque insertion | — | O(n) | O(1) en moyenne | O(log n) |
| `create_index` sur une table existante | — | O(n²) | O(n) en moyenne | O(n log n) |
| Recherche par égalité | O(n) | O(n) | O(1 + k) en moyenne | O(log n + k) |
| Recherche par intervalle | O(n) | O(n) | O(n) (parcours) | O(log n + k) |
| m conditions combinées (ET) | O(n · m) | O(n + k · m) | O(1 + k · m) en moyenne | O(log n + k · m) |
| Mémoire | O(n) | O(n) | O(n) | O(n) |

Autres opérations :
- `create_table_struct` : O(longueur de la définition) ;
- `get_value` : O(1) ;
- recherche d'une colonne par son nom : O(c).

Pire cas de la table de hachage : O(n) si toutes les clés tombent dans la même case, ce que la fonction de hachage rend très improbable.

L'insertion est en O(1) **amorti** grâce au doublement de la capacité. Avec une croissance linéaire (+16 lignes à chaque fois), elle passerait à O(n) amorti, soit O(n²) pour n insertions : voir l'analyse 2.

## Optimisations par rapport à une implémentation naïve

L'implémentation naïve de référence stocke les lignes, fait toujours un parcours complet et agrandit son stockage d'une taille fixe.

1. **Index** : une recherche par égalité passe de O(n) à O(1) avec la table de hachage, ou à O(log n) avec l'AVL. Une recherche par intervalle passe de O(n) à O(log n + k) avec l'AVL.
2. **Planificateur** : il choisit l'index le mieux adapté à chaque requête, et pour plusieurs conditions il ne filtre que les lignes candidates au lieu de toute la table.
3. **Stockage contigu à taille fixe** : pas d'allocation par ligne, accès en O(1), bonne utilisation du cache.
4. **Croissance géométrique (×2)** : insertion en O(1) amorti, au lieu de O(n) avec une croissance linéaire.
5. **Table de hachage soignée** : redimensionnement automatique (facteur de charge ≤ 1).
6. **Arbre équilibré (AVL)** : le temps en O(log n) est garanti, même pour des clés insérées dans l'ordre.
7. **Une seule entrée par clé distincte**, avec sa liste de lignes : les doublons ne font pas grossir la structure de recherche.
8. **Arrêts anticipés** : la liste triée s'arrête dès qu'elle dépasse la clé cherchée, et l'AVL ignore les sous-arbres hors de l'intervalle.

## Analyse de performance

Conditions de mesure :
- Windows 11, gcc (WinLibs, MinGW-w64), options `-O2` ;
- chaque valeur est la **médiane de 5 exécutions** ;
- table `users(id INT, name TEXT(15), age INT)`, avec des identifiants 0..N−1 insérés dans un **ordre aléatoire** ;
- l'index est créé avant les insertions, donc son coût est compté dans la construction ;
- 1000 recherches par égalité sur `id`, puis 200 recherches `id BETWEEN x AND x+99` (100 lignes chacune) ;
- le benchmark vérifie le nombre de lignes renvoyées par chaque requête.

### Analyse 1 : choix de la structure d'index

Implémentations comparées : optimales (table de hachage et AVL), sous-optimale volontaire (liste triée) et naïve (aucun index). Les temps sont en millisecondes.

**N = 10 000 lignes**

| Structure | Construction | 1000 recherches = | 200 recherches intervalle |
|---|---|---|---|
| Aucun index (parcours) | 1,92 | 159,68 | 42,68 |
| Liste triée (sous-optimal) | 293,65 | 77,20 | 15,95 |
| Table de hachage | 4,15 | 0,16 | 44,74 |
| Arbre AVL | 7,02 | 0,30 | 1,01 |

**N = 50 000 lignes**

| Structure | Construction | 1000 recherches = | 200 recherches intervalle |
|---|---|---|---|
| Aucun index (parcours) | 10,02 | 820,55 | 217,26 |
| Liste triée (sous-optimal) | 10 180,19 | 426,84 | 86,84 |
| Table de hachage | 16,72 | 0,16 | 162,68 |
| Arbre AVL | 28,56 | 0,31 | 1,23 |

**N = 200 000 lignes** (la liste triée est trop lente pour être mesurée)

| Structure | Construction | 1000 recherches = | 200 recherches intervalle |
|---|---|---|---|
| Aucun index (parcours) | 33,00 | 2 438,97 | 650,00 |
| Table de hachage | 97,99 | 0,55 | 641,18 |
| Arbre AVL | 177,04 | 0,77 | 3,44 |

Les résultats pour N = 1000 sont aussi affichés par `./bench`.

**Observations**

- **Égalité.** Quand N passe de 10 000 à 200 000 (×20), le parcours complet est environ 15 fois plus lent : c'est le comportement linéaire O(n) attendu. La table de hachage reste presque constante (0,16 → 0,55 ms pour 1000 requêtes, environ 0,5 µs par requête) ; la légère hausse vient des défauts de cache sur une structure plus grande. L'AVL augmente lentement (0,30 → 0,77 ms), comme prévu en O(log n). À 200 000 lignes, la table de hachage est environ **4400 fois plus rapide** que le parcours complet.
- **Intervalles.** Seul l'AVL est efficace : à 200 000 lignes, il met 3,4 ms contre 650 ms pour le parcours complet, soit environ 190 fois plus rapide. La table de hachage ne peut pas traiter les intervalles, donc le planificateur se rabat sur un parcours complet et le temps est le même.
- **Liste triée (sous-optimale).** Elle cumule les inconvénients :
  - sa construction est **quadratique** : N multiplié par 5 (10 000 → 50 000) donne un temps multiplié par 35 (0,29 s → 10,2 s), soit environ 356 fois plus lent que l'AVL ;
  - ses recherches ne sont qu'environ 2 fois plus rapides qu'un parcours complet, et environ 1400 fois plus lentes que l'AVL ;
  - elle est même moins efficace que le simple parcours des lignes, qui sont stockées de façon contiguë.
- **Coût des index à l'insertion.** Maintenir un index rend la construction 3 fois (hash) à 5 fois (AVL) plus lente que sans index. Ce coût est vite rentabilisé : à 200 000 lignes, il est remboursé dès quelques recherches.

**Conclusion.** La structure optimale dépend des requêtes :
- **table de hachage** pour des recherches par égalité ;
- **AVL** dès qu'il faut des intervalles ou des tris ;
- **pas d'index** pour une colonne rarement interrogée, car un index ralentit les insertions.

La liste triée montre qu'une structure « triée » mais sans accès direct ne suffit pas : c'est l'accès logarithmique ou constant qui fait le gain.

### Analyse 2 : croissance du stockage des lignes

Comparaison entre doublement de la capacité (optimal) et ajout de 16 lignes à chaque fois (sous-optimal). Table `logs(id INT, msg TEXT(47), code INT)` ; temps d'insertion de N lignes.

| N | Doublement : temps | Doublement : réallocations | Linéaire : temps | Linéaire : réallocations |
|---|---|---|---|---|
| 10 000 | 0,58 ms | 11 | 1,82 ms | 625 |
| 50 000 | 2,96 ms | 13 | 341,20 ms | 3 125 |
| 100 000 | 6,53 ms | 14 | 1 117,53 ms | 6 250 |

Quand N passe de 10 000 à 100 000 (×10) :
- le doublement prend environ 11 fois plus de temps, ce qui est linéaire : O(1) amorti par insertion ;
- la croissance linéaire prend environ 600 fois plus de temps : chaque réallocation peut recopier toute la table, ce qui donne O(n²) au total.

À 100 000 lignes, le doublement est **environ 170 fois plus rapide**, avec seulement 14 réallocations contre 6 250.

## Limites

- Pas de suppression ni de mise à jour de lignes, pas de persistance sur disque.
- Pas de jointure ni de SQL ; un seul index par colonne.
- Les colonnes `TEXT` ont une longueur maximale fixée dans la définition.
- La chaîne renvoyée par `get_value` pour un `TEXT` pointe dans la table : elle peut devenir invalide après une nouvelle insertion, si la table est réallouée.
- Pas de gestion de la concurrence (un seul thread).
