#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

int total_tests = 0;
int passed_tests = 0;

/*
 * test_dossier - Teste tous les fichiers .tpc d'un dossier.
 *
 * check_warning : si 1, vérifie qu'un message de warning est présent sur stderr
 *                 en plus du code de retour attendu (0).
 */
void test_dossier(char *path, int code_attendu, int check_warning) {
    DIR *d;
    struct dirent *dir;
    char commande[1024];

    d = opendir(path);
    if (!d) {
        printf("Impossible d'ouvrir %s\n", path);
        return;
    }

    printf("--- Test dans %s ---\n", path);

    while ((dir = readdir(d)) != NULL) {

        if (dir->d_type == DT_REG && strstr(dir->d_name, ".tpc")) {
            total_tests++;

            int code_retour = -1;

            if (check_warning) {
                /*
                 * Pour les warnings : on capture stderr dans un fichier temporaire
                 * afin de vérifier qu'un message de warning a bien été émis.
                 */
                char stderr_tmp[] = "/tmp/tpcc_stderr_XXXXXX";
                int fd = mkstemp(stderr_tmp);
                if (fd == -1) {
                    printf("[ERREUR] %s : impossible de créer le fichier temporaire\n", dir->d_name);
                    continue;
                }
                close(fd);

                snprintf(commande, sizeof(commande),
                         "./bin/tpcc < %s/%s > /dev/null 2> %s",
                         path, dir->d_name, stderr_tmp);

                int ret = system(commande);
                if (WIFEXITED(ret))
                    code_retour = WEXITSTATUS(ret);

                /* Lecture du fichier stderr pour détecter un warning */
                int has_warning = 0;
                FILE *f = fopen(stderr_tmp, "r");
                if (f) {
                    char line[512];
                    while (fgets(line, sizeof(line), f)) {
                        /* On cherche le mot "warning" (insensible à la casse) */
                        char lower[512];
                        int i;
                        for (i = 0; line[i] && i < (int)sizeof(lower) - 1; i++)
                            lower[i] = (char)((line[i] >= 'A' && line[i] <= 'Z')
                                              ? line[i] + 32 : line[i]);
                        lower[i] = '\0';
                        if (strstr(lower, "warning")) {
                            has_warning = 1;
                            break;
                        }
                    }
                    fclose(f);
                }
                unlink(stderr_tmp);

                if (code_retour == code_attendu && has_warning) {
                    printf("[OK]     %s\n", dir->d_name);
                    passed_tests++;
                } else if (code_retour != code_attendu && !has_warning) {
                    printf("[ERREUR] %s : attendu code %d + warning, eu code %d sans warning\n",
                           dir->d_name, code_attendu, code_retour);
                } else if (code_retour != code_attendu) {
                    printf("[ERREUR] %s : attendu code %d, eu %d (warning présent)\n",
                           dir->d_name, code_attendu, code_retour);
                } else {
                    printf("[ERREUR] %s : code %d correct mais aucun warning détecté\n",
                           dir->d_name, code_attendu);
                }

            } else {
                /* Cas standard : on ignore stdout et stderr, on vérifie juste le code */
                snprintf(commande, sizeof(commande),
                         "./bin/tpcc < %s/%s > /dev/null 2>&1",
                         path, dir->d_name);

                int ret = system(commande);
                if (WIFEXITED(ret))
                    code_retour = WEXITSTATUS(ret);

                if (code_retour == code_attendu) {
                    printf("[OK]     %s\n", dir->d_name);
                    passed_tests++;
                } else {
                    printf("[ERREUR] %s : attendu %d, eu %d\n",
                           dir->d_name, code_attendu, code_retour);
                }
            }
        }
    }
    closedir(d);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Vérification que le compilateur est bien compilé */
    if (access("./bin/tpcc", F_OK) == -1) {
        printf("Erreur: ./bin/tpcc introuvable. Fais un make !\n");
        return 1;
    }

    /* good     : code 0, pas de warning attendu        */
    test_dossier("./test/good",    0, 0);
    printf("\n");

    /* warn     : code 0, avec au moins un warning      */
    test_dossier("./test/warn",    0, 1);
    printf("\n");

    /* syn-err  : code 1                                */
    test_dossier("./test/syn-err", 1, 0);
    printf("\n");

    /* sem-err  : code 2                                */
    test_dossier("./test/sem-err", 2, 0);

    printf("\n----------------\n");
    printf("Résultat final : %d/%d tests passés.\n", passed_tests, total_tests);

    return (passed_tests == total_tests) ? 0 : 1;
}