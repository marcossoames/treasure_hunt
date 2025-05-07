// COMPILARE: gcc -Wall -o tb treasure_hub.c
// RULARE: ./tb

/* DESCRIERE:

PHASE 2:
start_monitor -> porneste monitorul
stop_monitor -> opreste monitorul
exit -> daca monitorul inca ruleaza trimite un mesaj de eroare, altfel iese din treasure_hub
list_hunts -> lista directoarelor de tip hunt si pentru fiecare director afiseaza numarul de comori
list_treasures <hunt_id> -> afiseaza toate datele comorilor dintr-un hunt specificat
view_treasure -> afiseaza informatii despre un treasure dintr-un hunt

handler = functie care se apeleaza automat la primirea unui semnal; se leaga de sigaction
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>    // Semnale
#include <sys/wait.h>  // Wait si sleep
#include <dirent.h>    // Structura pentru a citi un director (list_hunts)
#include <fcntl.h>     // Fisiere
#include <sys/types.h> // pid_t
#include <sys/stat.h>  // Permisiuni open()

#define BUFFER_SIZE 128

pid_t monitor_pid = -1; // PID-ul procesului monitor (-1 inseamna ca este inexistent)

// Structura comoara
struct Treasure
{
    int id;
    char user_name[50];
    float GPS_lat;
    float GPS_long;
    char clue[50];
    int value;
} t;

// Handler pentru semnalul SIGUSR1 – doarme pana la primirea semnalului; folosit in procesul monitor
void sigusr1_handler(int signum)
{
    char *mesaj_asteptare = "[MONITOR] Primit semnal de oprire. Se inchide in 3 secunde...\n";
    write(1, mesaj_asteptare, strlen(mesaj_asteptare));
    usleep(3000000); // Asteapta 3 secunde
    char *mesaj_inchidere = "[MONITOR] Monitorul se inchide acum.\n";
    write(1, mesaj_inchidere, strlen(mesaj_inchidere));
    exit(0); // Procesul monitor iese
}

// Handler pentru SIGCHLD – rulat automat cand monitorul se inchide
void sigchld_handler(int signum)
{
    int status;
    wait(&status); // Eliberam memoria ocupata de procesul monitor
    // Verific daca monitorul s-a inchis cu succes
    monitor_pid = -1;
    char *mesaj_inchidere_monitor = "Monitorul s-a inchis.\n";
    write(1, mesaj_inchidere_monitor, strlen(mesaj_inchidere_monitor));
}

// Handler pentru SIGUSR2 - list_hunts
void sigusr2_handler(int signum)
{
    DIR *dir = opendir("."); // Parcurg directorul curent
    if (dir == NULL)
    {
        char *mesaj_eroare_director = "[MONITOR] Eroare la deschiderea directorului curent.\n";
        write(1, mesaj_eroare_director, strlen(mesaj_eroare_director));
        exit(-1);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Caut directoare care încep cu "hunt"
        if (strncmp(entry->d_name, "hunt", 4) == 0)
        {
            DIR *hunt_dir = opendir(entry->d_name);
            if (hunt_dir == NULL)
            {
                continue; // Sar peste directoarele care nu pot fi deschise
            }

            // Numar comorile din directorul hunt curent
            int k = 0;
            struct dirent *subentry;
            while ((subentry = readdir(hunt_dir)) != NULL)
            {
                if ((strncmp(subentry->d_name, "comoara", 7) == 0) && (strstr(subentry->d_name, ".bin")))
                {
                    k++;
                }
            }
            closedir(hunt_dir);

            char buf[256];
            snprintf(buf, sizeof(buf), "[MONITOR] %s: %d comori\n", entry->d_name, k);
            write(1, buf, strlen(buf));
        }
    }

    closedir(dir);
}

// Handler pentru SIGHUP - list_treasures
void sig_list_treasures_handler(int signum)
{
    char hunt_id[128];
    memset(hunt_id, 0, sizeof(hunt_id));

    int fd = open("hunt_id.tmp", O_RDONLY);
    if (fd < 0)
    {
        char *eroare_deschidere_fisier = "[MONITOR] Eroare la deschiderea fisierului hunt_id.tmp\n";
        write(1, eroare_deschidere_fisier, strlen(eroare_deschidere_fisier));
        return;
    }

    read(fd, hunt_id, sizeof(hunt_id) - 1); // Citesc hunt_id din fișier
    close(fd);

    // Construimesc calea catre directorul hunt
    DIR *dir = opendir(hunt_id);
    if (dir == NULL)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "[MONITOR] Directorul '%s' nu exista sau nu poate fi deschis.\n", hunt_id);
        write(1, msg, strlen(msg));
        exit(-1);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if ((strncmp(entry->d_name, "comoara", 7) == 0) && (strstr(entry->d_name, ".bin"))) // Verific ca fisierul sa fie unul de tip: comoaraX.bin
        {
            char path[256];
            snprintf(path, sizeof(path), "%s/%s", hunt_id, entry->d_name);
            int fd_comoara = open(path, O_RDONLY);
            if (fd_comoara < 0) // Daca deschiderea fisierului a esuat
            {
                continue;
            }

            read(fd_comoara, &t, sizeof(t));
            close(fd_comoara);

            char buf[512];
            snprintf(buf, sizeof(buf), "[MONITOR] ID: %d | User: %s | GPS: %.2f, %.2f | Valoare: %d | Indiciu: %s\n", t.id, t.user_name, t.GPS_lat, t.GPS_long, t.value, t.clue);
            write(1, buf, strlen(buf));
        }
    }

    closedir(dir);
}

void sig_view_treasure_handler(int signum)
{
    int fd = open("view_treasure.tmp", O_RDONLY);
    if (fd < 0)
    {
        char *mesaj_deschidere_fisier = "[MONITOR] Eroare la deschiderea fisierului treasure.tmp.\n";
        write(1, mesaj_deschidere_fisier, strlen(mesaj_deschidere_fisier));
        return;
    }

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    // Extrag ID-ul comorii din buffer impartind continutul fisierului temporar in 2 parti hunt_id si treasure_id
    // Hunt_id este tot ce vine inainte de linie noua, iar treasure_id este tot ce vine dupa
    char *hunt_id = strtok(buffer, "\n");
    char *treasure_id = strtok(NULL, "\n");
    if ((hunt_id == NULL) || (treasure_id == NULL))
    {
        char *mesaj_eroare = "[MONITOR] Format invalid in fisierul temporar.\n";
        write(1, mesaj_eroare, strlen(mesaj_eroare));
        exit(-1);
    }

    // Creez calea catre fisierul comoaraX.bin
    // Hunt_id este numele directorului, iar treasure_id este numele fisierului
    char path[256];
    snprintf(path, sizeof(path), "%s/comoara%s.bin", hunt_id, treasure_id);
    int fd_comoara = open(path, O_RDONLY); // Am nevoie doar sa pot citi din fisier
    if (fd_comoara < 0)                    // Daca deschiderea fisierului a esuat
    {
        char mesaj_eroare_comoara[256];
        snprintf(mesaj_eroare_comoara, sizeof(mesaj_eroare_comoara), "[MONITOR] Comoara %s din %s nu exista sau nu poate fi deschisa.\n", treasure_id, hunt_id);
        write(1, mesaj_eroare_comoara, strlen(mesaj_eroare_comoara));
        exit(-1);
    }

    read(fd_comoara, &t, sizeof(t));
    close(fd_comoara);

    char output[512];
    snprintf(output, sizeof(output), "[MONITOR] ID: %d | User: %s | GPS: %.2f, %.2f | Valoare: %d | Indiciu: %s\n", t.id, t.user_name, t.GPS_lat, t.GPS_long, t.value, t.clue);
    write(1, output, strlen(output));
}

// Functie pentru pornirea procesului monitor
// LAB 6
void start_monitor()
{
    if (monitor_pid != -1)
    {
        char *mesaj_monitor = "Monitorul este deja pornit.\n";
        write(1, mesaj_monitor, strlen(mesaj_monitor));
        exit(-1); // Ies daca monitorul este deja activ
    }

    pid_t pid = fork(); // Creez un nou proces (monitor)

    if (pid < 0)
    {
        char *eroare_proces = "Eroare la fork.\n";
        write(1, eroare_proces, strlen(eroare_proces));
        exit(-1); // Eroare la fork
    }

    if (pid == 0)
    {
        // Handler pentru SIGUSR1 (oprire monitor)
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = sigusr1_handler;
        sigaction(SIGUSR1, &act, NULL);

        // Handler pentru SIGUSR2 (listare hunt-uri)
        struct sigaction act2;
        memset(&act2, 0, sizeof(act2));
        act2.sa_handler = sigusr2_handler;
        sigaction(SIGUSR2, &act2, NULL);

        // Handler pentru SIGRTMIN (primul semnal personalizat dupa SIGUSR1/2) – list_treasures
        // SIGRTMIN nu este disponibil pe MAC, asa ca folosesc SIGHUP
        struct sigaction act3;
        memset(&act3, 0, sizeof(act3));
        act3.sa_handler = sig_list_treasures_handler;
        sigaction(SIGHUP, &act3, NULL);

        // Handler pentru SIGTERM - view_treasure
        // Semnal de oprire, dar poate fi interceptat in procesul monitor pentru a afisa informatiile despre comoara
        struct sigaction act4;
        memset(&act4, 0, sizeof(act4));
        act4.sa_handler = sig_view_treasure_handler;
        sigaction(SIGTERM, &act4, NULL);

        // Proces copil = monitor
        // sleep(1); // Astept 1 secunda pentru a permite procesului parinte sa continue
        char *mesaj_monitor_succes = "[MONITOR] Pornit. Se asteapta semnale...(SIGACTION)\n\n";
        write(1, mesaj_monitor_succes, strlen(mesaj_monitor_succes));

        // Asteapta semnale intr-o bucla infinita
        while (1)
        {
            pause(); // Procesul copil asteapta un semnal
        }
    }
    else
    {
        // Proces parinte
        monitor_pid = pid;
        char *mesaj_functionare = "Monitorul a fost pornit cu succes.\n";
        write(1, mesaj_functionare, strlen(mesaj_functionare));
    }
}

int main(void)
{
    // Setez handler pentru semnalul SIGCHLD (cand monitorul moare)
    struct sigaction act_chld; // Actiune asociata semnalului SIGCHLD
    memset(&act_chld, 0, sizeof(act_chld));
    act_chld.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &act_chld, NULL);

    char buffer[BUFFER_SIZE];

    while (1) // Bucla infinita pentru a astepta comenzi
    {
        // Afisez promptul cu comenzile disponibile
        char *comanda = "\nDati comanda (start_monitor / list_hunts / list_treasures <hunt_id> / view_treasure <hunt_id> <id> / stop_monitor / exit):\n\n";
        write(1, comanda, strlen(comanda));

        // Citesc comanda de la utilizator
        memset(buffer, 0, sizeof(buffer));
        read(0, buffer, sizeof(buffer));

        // Curat newline-ul de la final
        char *newline = strchr(buffer, '\n');
        if (newline)
        {
            *newline = '\0';
        }

        if (strcmp(buffer, "start_monitor") == 0)
        {
            start_monitor();
        }
        else if (strcmp(buffer, "stop_monitor") == 0)
        {
            if (monitor_pid == -1)
            {
                char *mesaj_eroare = "\nEroare: Monitorul nu este activ.\n";
                write(1, mesaj_eroare, strlen(mesaj_eroare));
            }
            else
            {
                kill(monitor_pid, SIGUSR1);
                char *mesaj_oprire = "\nSemnal trimis monitorului pentru a se opri.\n";
                write(1, mesaj_oprire, strlen(mesaj_oprire));
            }
        }
        else if (strcmp(buffer, "list_hunts") == 0)
        {
            if (monitor_pid == -1)
            {
                char *mesaj_eroare_hunts = "\nEroare: Monitorul nu este activ.\n";
                write(1, mesaj_eroare_hunts, strlen(mesaj_eroare_hunts));
            }
            else
            {
                char *mesaj_listare_hunturi = "\nCerere de listare hunt-uri trimisa monitorului.\n";
                write(1, mesaj_listare_hunturi, strlen(mesaj_listare_hunturi));
                kill(monitor_pid, SIGUSR2);
                usleep(1000000); // Astept 1 secunda pentru a permite monitorului sa proceseze semnalul
                /*char *mesaj_listare_hunturi = "Cerere de listare hunt-uri trimisa monitorului.\n";
                write(1, mesaj_listare_hunturi, strlen(mesaj_listare_hunturi)); -> Le mut inainte de kill pentru lizibilitate */
            }
        }
        else if (strcmp(buffer, "exit") == 0)
        {
            if (monitor_pid != -1)
            {
                char *mesaj_eroare = "\nEroare: Monitorul este inca activ. Foloseste stop_monitor.\n";
                write(1, mesaj_eroare, strlen(mesaj_eroare));
            }
            else
            {
                char *mesaj_iesire = "\nIesire din treasure_hub.\n\n";
                write(1, mesaj_iesire, strlen(mesaj_iesire));
                break;
            }
        }
        else
        {
            if (strncmp(buffer, "list_treasures ", 15) == 0)
            {
                if (monitor_pid == -1)
                {
                    char *eroare = "\nEroare: Monitorul nu este activ.\n";
                    write(1, eroare, strlen(eroare));
                }
                else
                {
                    char *hunt_id = buffer + 15;
                    int fd = open("hunt_id.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0)
                    {
                        char *eroare_scriere_fisier = "\nEroare la scrierea in fisierul temporar.\n";
                        write(1, eroare_scriere_fisier, strlen(eroare_scriere_fisier));
                    }
                    else
                    {
                        write(fd, hunt_id, strlen(hunt_id));
                        close(fd);
                        char *mesaj_listare_comori = "\nCerere de listare comori trimisa monitorului.\n";
                        write(1, mesaj_listare_comori, strlen(mesaj_listare_comori));
                        kill(monitor_pid, SIGHUP);
                        usleep(1000000); // Astept 1 secunda pentru a permite monitorului sa proceseze semnalul
                        /*char *mesaj_listare_comori = "Cerere de listare comori trimisa monitorului.\n";
                        write(1, mesaj_listare_comori, strlen(mesaj_listare_comori));*/
                    }
                }
            }
            else if (strncmp(buffer, "view_treasure ", 14) == 0)
            {
                if (monitor_pid == -1)
                {
                    char *eroare = "\nEroare: Monitorul nu este activ.\n";
                    write(1, eroare, strlen(eroare));
                }
                else
                {
                    char *rest = buffer + 14;         // Sar peste "view_treasure "
                    char *spatiu = strchr(rest, ' '); // Caut spatiul dintre hunt_id si id_comoara
                    if (spatiu == NULL)
                    {
                        char *mesaj_argumente = "\nEroare: Trebuie sa specifici hunt_id si id_comoara.\n";
                        write(1, mesaj_argumente, strlen(mesaj_argumente));
                    }
                    else
                    {
                        *spatiu = '\0';                // Inlocuiesc spatiul cu terminatorul de sir
                        char *hunt_id = rest;          // Hunt_id este tot ce vine inainte de spatiu
                        char *id_comoara = spatiu + 1; // id_comoara este tot ce vine dupa spatiu

                        int fd = open("view_treasure.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644); // Creez fisierul temporar
                        if (fd < 0)
                        {
                            char *eroare_fisier = "\nEroare la scrierea fisierului temporar.\n";
                            write(1, eroare_fisier, strlen(eroare_fisier));
                        }
                        else
                        {
                            dprintf(fd, "%s\n%s\n", hunt_id, id_comoara);
                            close(fd);
                            char *mesaj = "\nCerere de afisare comoara trimisa monitorului.\n";
                            write(1, mesaj, strlen(mesaj));
                            kill(monitor_pid, SIGTERM);
                            usleep(1000000); // Astept 1 secunda pentru a permite monitorului sa proceseze semnalul
                            /*char *mesaj = "Cerere de afisare comoara trimisa monitorului.\n";
                            write(1, mesaj, strlen(mesaj));*/
                        }
                    }
                }
            }
            else
            {
                char *mesaj_comanda_necunoscuta = "Comanda necunoscuta.\n";
                write(1, mesaj_comanda_necunoscuta, strlen(mesaj_comanda_necunoscuta));
            }
        }
    }

    return 0;
}
