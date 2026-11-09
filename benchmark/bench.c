#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "minidb.h"

#define REPETITIONS 5
#define NB_EGALITE  1000
#define NB_INTERVAL 200
#define LARGEUR     100
#define LISTE_MAX   50000

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static uint64_t etat = 2024;
static uint64_t rnd(void) {
    etat ^= etat << 13;
    etat ^= etat >> 7;
    etat ^= etat << 17;
    return etat;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double mediane(double t[], int n) {
    qsort(t, n, sizeof *t, cmp_double);
    return n % 2 ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) / 2;
}

typedef struct { double build, eq, range; } Mesure;

static int verif_ok = 1;

static Mesure mesurer(IndexKind kind, const int64_t *ids, int n,
                      const int64_t *cles, const int64_t *bornes) {
    double tb[REPETITIONS], te[REPETITIONS], tr[REPETITIONS];
    char nom[16];

    for (int rep = 0; rep < REPETITIONS; rep++) {
        Database *db = db_create();
        Table *t = create_table_struct(db, "users(id INT, name TEXT(15), age INT)");
        if (kind != INDEX_NONE) create_index(t, "id", kind);

        double d = now();
        for (int i = 0; i < n; i++) {
            snprintf(nom, sizeof nom, "user%lld", (long long)ids[i]);
            Value row[] = { VAL_INT(ids[i]), VAL_TEXT(nom), VAL_INT(ids[i] % 100) };
            insert_row(t, row, 3);
        }
        tb[rep] = now() - d;

        size_t total = 0;
        d = now();
        for (int q = 0; q < NB_EGALITE; q++) {
            Condition c = { "id", OP_EQ, VAL_INT(cles[q]), {0} };
            ResultSet rs = select_values(t, &c, 1);
            total += rs.count;
            result_free(&rs);
        }
        te[rep] = now() - d;
        if (total != NB_EGALITE) verif_ok = 0;

        total = 0;
        d = now();
        for (int q = 0; q < NB_INTERVAL; q++) {
            Condition c = { "id", OP_BETWEEN, VAL_INT(bornes[q]),
                            VAL_INT(bornes[q] + LARGEUR - 1) };
            ResultSet rs = select_values(t, &c, 1);
            total += rs.count;
            result_free(&rs);
        }
        tr[rep] = now() - d;
        if (total != (size_t)NB_INTERVAL * LARGEUR) verif_ok = 0;

        db_free(db);
    }
    Mesure m = { mediane(tb, REPETITIONS), mediane(te, REPETITIONS),
                 mediane(tr, REPETITIONS) };
    return m;
}

static void bench_index(int n) {
    int64_t *ids = malloc(n * sizeof *ids);
    int64_t cles[NB_EGALITE], bornes[NB_INTERVAL];

    for (int i = 0; i < n; i++) ids[i] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(rnd() % (uint64_t)(i + 1));
        int64_t tmp = ids[i]; ids[i] = ids[j]; ids[j] = tmp;
    }
    for (int q = 0; q < NB_EGALITE; q++) cles[q] = (int64_t)(rnd() % (uint64_t)n);
    for (int q = 0; q < NB_INTERVAL; q++) bornes[q] = (int64_t)(rnd() % (uint64_t)(n - LARGEUR));

    printf("\n### N = %d lignes\n\n", n);
    printf("| Structure | Construction (ms) | %d recherches = (ms) | %d recherches intervalle (ms) |\n",
           NB_EGALITE, NB_INTERVAL);
    printf("|---|---|---|---|\n");

    IndexKind kinds[] = { INDEX_NONE, INDEX_LIST, INDEX_HASH, INDEX_AVL };
    const char *noms[] = { "Aucun index (parcours)", "Liste triee (sous-optimal)",
                           "Table de hachage", "Arbre AVL" };
    for (int k = 0; k < 4; k++) {
        if (kinds[k] == INDEX_LIST && n > LISTE_MAX) {
            printf("| %s | trop lent | - | - |\n", noms[k]);
            continue;
        }
        Mesure m = mesurer(kinds[k], ids, n, cles, bornes);
        printf("| %s | %.2f | %.2f | %.2f |\n", noms[k],
               m.build * 1e3, m.eq * 1e3, m.range * 1e3);
        fflush(stdout);
    }
    free(ids);
}

static void bench_croissance(int n) {
    GrowthPolicy pol[] = { GROWTH_DOUBLING, GROWTH_LINEAR };
    const char *noms[] = { "Doublement (x2)", "Lineaire (+16 lignes)" };

    printf("\n### Croissance du stockage, N = %d lignes\n\n", n);
    printf("| Strategie | Insertion (ms) | Reallocations |\n|---|---|---|\n");
    for (int p = 0; p < 2; p++) {
        double t[REPETITIONS];
        size_t reallocs = 0;
        for (int rep = 0; rep < REPETITIONS; rep++) {
            Database *db = db_create();
            Table *tb = create_table_struct(db, "logs(id INT, msg TEXT(47), code INT)");
            table_set_growth(tb, pol[p]);
            double d = now();
            for (int i = 0; i < n; i++) {
                Value row[] = { VAL_INT(i), VAL_TEXT("message de journal"), VAL_INT(i % 7) };
                insert_row(tb, row, 3);
            }
            t[rep] = now() - d;
            reallocs = tb->realloc_count;
            db_free(db);
        }
        printf("| %s | %.2f | %zu |\n", noms[p], mediane(t, REPETITIONS) * 1e3, reallocs);
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    int faire_index = argc < 2 || strcmp(argv[1], "index") == 0;
    int faire_croissance = argc < 2 || strcmp(argv[1], "croissance") == 0;

    printf("## Benchmark minidb (mediane de %d executions)\n", REPETITIONS);

    if (faire_index) {
        int tailles[] = { 1000, 10000, 50000, 200000 };
        for (int i = 0; i < 4; i++) bench_index(tailles[i]);
    }
    if (faire_croissance) {
        int tailles[] = { 10000, 50000, 100000 };
        for (int i = 0; i < 3; i++) bench_croissance(tailles[i]);
    }

    printf("\nVerification des resultats : %s\n", verif_ok ? "OK" : "ERREUR");
    return verif_ok ? 0 : 1;
}
