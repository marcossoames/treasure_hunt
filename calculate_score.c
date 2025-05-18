/*
// COMPILARE: gcc -Wall -o cs calculate_score.c

DESCRIERE:

PHASE 3:
procesul monitor nu mai este pornit de catre utilizator, ci de catre treasure_hub.c (folosesc pipe)
adaug comanda pentru calculate_score pentru a calcula scorul total al unui hunt (suma valorilor comorilor)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> // Biblioteca pentru readdir()
#include <fcntl.h>  // Biblioteca pentru open()
#include <unistd.h>

typedef struct
{
    int id;
    char user_name[50];
    float lat, lon;
    char clue[50];
    int value;
} Treasure;

typedef struct
{
    char user[50];
    int total_value;
} Score;

#define MAX_USERS 100

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        char *eroare_numar_argumente = "Numar gresit de argumente!\n";
        write(1, eroare_numar_argumente, strlen(eroare_numar_argumente));
        exit(-1);
    }

    int i;

    char *hunt_dir = argv[1];     // Directorul in care caut fisierele de tip comoaraX.bin
    DIR *dir = opendir(hunt_dir); // Deschid directorul
    if (dir == NULL)              // Verific daca directorul a fost deschis cu succes
    {
        char *eroare_creare_director = "Directorul nu a fost creat cu succes!\n";
        write(1, eroare_creare_director, strlen(eroare_creare_director));
        exit(-1);
    }

    Score scores[MAX_USERS]; // Array pentru a stoca scorurile
    int score_count = 0;     // Contor pentru numarul de utilizatori

    // Parcurg toate fisierele din directorul hunt si caut fisierele de tip comoaraX.bin
    // LAB 7
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if ((strncmp(entry->d_name, "comoara", 7) == 0) && (strstr(entry->d_name, ".bin")))
        {
            char path[256];                                                 // Calea catre fisierul comoaraX.bin
            snprintf(path, sizeof(path), "%s/%s", hunt_dir, entry->d_name); // Construiesc calea completa

            int fd = open(path, O_RDONLY); // Deschid fisierul pentru citire
            if (fd < 0)                    // Daca deschiderea fisierului a esuat
            {
                char *eroare_deschidere_fisier = "Eroare la deschiderea fisierului!\n";
                write(1, eroare_deschidere_fisier, strlen(eroare_deschidere_fisier));
                continue;
            }

            Treasure t;                     // Structura pentru a stoca datele comorii
            read(fd, &t, sizeof(Treasure)); // Citesc datele din fisierul comoaraX.bin
            close(fd);                      // Inchid fisierul dupa citire

            int gasit = 0; // Flag pentru a verifica daca utilizatorul exista deja in scoruri
            for (i = 0; i < score_count; i++)
            {
                if (strcmp(scores[i].user, t.user_name) == 0)
                {
                    scores[i].total_value = scores[i].total_value + t.value; // Adaug valoarea comorii la scorul utilizatorului
                    gasit = 1;
                    break;
                }
            }
            if ((gasit == 0) && (score_count < MAX_USERS)) // Daca utilizatorul nu exista in scoruri
            {
                // Adaug utilizatorul in lista de scoruri
                strcpy(scores[score_count].user, t.user_name);
                scores[score_count].total_value = t.value;
                score_count++;
            }
        }
    }

    closedir(dir);

    printf("Scoruri pentru %s: ", hunt_dir);
    for (i = 0; i < score_count; i++)
    {
        printf("%s: %d\n\n", scores[i].user, scores[i].total_value);
    }
    return 0;
}