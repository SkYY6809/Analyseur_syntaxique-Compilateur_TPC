#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

int total_tests = 0;
int passed_tests = 0;

void test_dossier(char *path, int code_attendu) {
    DIR *d;
    struct dirent *dir;
    char commande[1024]; // Taille suffisante pour la commande
    
    d = opendir(path);
    if (!d) {
        printf("Impossible d'ouvrir %s\n", path);
        return;
    }

    printf("--- Test dans %s ---\n", path);

    while ((dir = readdir(d)) != NULL) {

        if (dir->d_type == DT_REG && strstr(dir->d_name, ".tpc")) {
            total_tests++;
            
            sprintf(commande, "./bin/tpcc < %s/%s > /dev/null 2>&1", path, dir->d_name);
            
            int ret = system(commande);
            int code_retour = -1;

            if (WIFEXITED(ret)) {
                code_retour = WEXITSTATUS(ret);
            }

            if (code_retour == code_attendu) {
                printf("[OK] %s\n", dir->d_name);
                passed_tests++;
            } else {
                printf("[ERREUR] %s : attendu %d, eu %d\n", dir->d_name, code_attendu, code_retour);
            }
        }
    }
    closedir(d);
}

int main(int argc, char **argv) {
    // Vérification de si l'exécutable est là
    if (access("./bin/tpcc", F_OK) == -1) {
        printf("Erreur: ./bin/tpcc introuvable. Fais un make !\n");
        return 1;
    }

    // On lance les tests directement
    test_dossier("./test/good", 0);
    printf("\n");
    test_dossier("./test/syn-err", 1);

    printf("\n----------------\n");
    printf("Résultat final : %d/%d tests passés.\n", passed_tests, total_tests);

    // Retourne 0 si tout est bon, 1 sinon
    return (passed_tests == total_tests) ? 0 : 1;
}
