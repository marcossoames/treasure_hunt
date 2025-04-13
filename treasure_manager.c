// COMPILARE: gcc -Wall -o t treasure_manager.c
// RULARE: ./t add hunt_id -> adaugare hunt-uri si comori SAU ./t list hunt_id -> afisare comori dintr-un hunt SAU ./t view hunt_id -> afisare date comori dintr-un hunt SAU ./t remove_treasure hunt_id -> stergere comoara dintr-un hunt SAU ./t remove_hunt hunt_id -> stergere hunt

/* DESCRIERE:

PHASE 1:
In folderul curent (in care este si executabilul) se vor crea directoare cu numele hunt_id (hunt1, hunt 2 etc.).
Ele vor fi date ca argument in linie de comanda.
Utilizatorul poate alege sa introduca sau nu comori intr-un hunt.
Datele comorilor adaugate vor fi date la tastatura si vor fi salvate in fisiere de tipul treasure.bin.
Se pot vizualiza datele tuturor comorilor dintr-un hunt (parcurgand reursiv toate fisierele cu extensia .bin din directorul respectiv)
plus data ultimei modificari si dimensiunea fisierului.
In fiecare director hunt se va crea si un fisier log de tipul logged_hunt.txt in care vor fi salvate opeatiile efectuate de user asupra hunt-ului respectiv.
In directorul treasure_hunt se vor crea symbolic link-uri catre fisierele logged_hunt de forma logged_hunt-<ID>.

PHASE 2:
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>    // stat
#include <sys/stat.h> // stat
#include <unistd.h>   // stat
#include <dirent.h>   // citirea directorului
#include <time.h>     // pentru a obtine data curenta

#define MAX_USER_NAME 50
#define MAX_CLUE 50

// Treasure data
typedef struct
{
  int id;
  char user_name[MAX_USER_NAME];
  float GPS_latitude;
  float GPS_longitude;
  char clue[MAX_CLUE];
  int value;
} Treasure;

// Creez un nou director pentru fiecare hunt
// LAB 6
int create_hunt_directory(const char *hunt_id)
{

  if ((hunt_id == NULL) || (strlen(hunt_id) == 0))
  {
    char *invalid_hunt = "Hunt id este invalid!\n";
    write(1, invalid_hunt, strlen(invalid_hunt));
    return 1;
  }

  // Verific daca directorul exista deja
  struct stat ab;
  if (stat(hunt_id, &ab) == 0)
  {
    char *director_exista = "Directorul exista deja!\n";
    write(1, director_exista, strlen(director_exista));
    // return 1;
    return 0; // Chiar daca directorul exista, permit continuarea programului pentru a avea mai tarziu optiunea de a adauga comori
  }

  // Creez directorul cu permisiuni depline
  if (mkdir(hunt_id, 0755) != 0)
  {
    char *eroare_creare_director = "Eroare la crearea directorului!\n";
    write(1, eroare_creare_director, strlen(eroare_creare_director));
    return 1;
  }

  char *succes_creare_director = "Directorul a fost creat cu succes!\n";
  write(1, succes_creare_director, strlen(succes_creare_director));
  return 0;
}

// Verific cate comori exista deja in directorul hunt_id
int existing_treasure_count(const char *hunt_id)
{
  DIR *dir = opendir(hunt_id);
  if (dir == NULL)
  {
    return 0;
  }

  int max_index = 0;
  struct dirent *intrare; // struct standard pentru functia readdir

  while ((intrare = readdir(dir)) != NULL)
  {
    if (strncmp(intrare->d_name, "comoara", 7) == 0 && strstr(intrare->d_name, ".bin"))
    {
      int nr = 0;
      sscanf(intrare->d_name, "comoara%d.bin", &nr);
      if (nr > max_index)
      {
        max_index = nr;
      }
    }
  }

  closedir(dir);
  return max_index;
}

// Functie de logare a operatiunilor efectuate asupra unui hunt
void log_operation(const char *hunt_id, const char *operatie)
{
  char log_path[256];
  memset(log_path, 0, sizeof(log_path));

  // Construim calea catre log
  strcpy(log_path, hunt_id);
  strcat(log_path, "/logged_hunt.txt");

  FILE *f = fopen(log_path, "a");
  if (f == NULL)
  {
    char *eroare_deschidere_fisier = "Eroare la deschiderea fisierului de log.\n";
    write(1, eroare_deschidere_fisier, strlen(eroare_deschidere_fisier));
    exit(-1);
  }

  // Obținem timpul curent
  time_t now = time(NULL);
  char timp[64];
  strftime(timp, sizeof(timp), "%Y-%m-%d %H:%M:%S", localtime(&now));

  // Construim linia de log
  char linie[512];
  strcpy(linie, "[");
  strcat(linie, timp);
  strcat(linie, "] ");
  strcat(linie, operatie);
  strcat(linie, "\n");

  // Scriem linia in fisier
  fwrite(linie, 1, strlen(linie), f);

  fclose(f);
}

// Functie pentru symlink-uri
// LAB 4 (Apeluri sistem pentru lucrul cu fisiere)
void create_symlink(const char *hunt_id)
{
  char symlink_path[256];
  memset(symlink_path, 0, sizeof(symlink_path));

  // Construiesc calea catre symlink
  strcpy(symlink_path, "logged_hunt-");
  strcat(symlink_path, hunt_id);
  strcat(symlink_path, ".txt");

  // Verific daca symlink-ul exista deja
  struct stat st; // struct standard pentru atributele unui fisier
  if (lstat(symlink_path, &st) == 0)
  {
    // Symlink-ul exista deja
    char *eroare_symlink = "Symlink-ul exista deja!\n";
    write(1, eroare_symlink, strlen(eroare_symlink));
    exit(-1);
  }

  // Creez symlink-ul
  if (symlink(hunt_id, symlink_path) != 0)
  {
    char *eroare_creare_symlink = "Eroare la crearea symlink-ului.\n";
    write(1, eroare_creare_symlink, strlen(eroare_creare_symlink));
    exit(-1);
  }
}

// Creez fisier binar pentru comoara
// LAB 6
int add_treasure_file(const char *hunt_id, Treasure treasure)
{
  char buffer[128]; // buffer pentru citirea datelor, pt a nu folosi scanf
  // setez buffer-ul pe 0, citesc cate un set de date, apoil il pun iar pe 0 si o o iau de la capat pana citesc toate datele de care am nevoie
  char raspuns[8];                                           // buffer pentru raspuns
  int treasure_count = existing_treasure_count(hunt_id) + 1; // contor pentru comori
  // fac o functie separata pentru a mentine numarul de comori din fiecare folder hunt

  while (1)
  {
    char *raspuns_adaugare_comoara = "Doriti sa adaugati o comoara? (da/nu): ";
    write(1, raspuns_adaugare_comoara, strlen(raspuns_adaugare_comoara));
    memset(raspuns, 0, sizeof(raspuns));
    read(0, raspuns, sizeof(raspuns));
    if (strcmp(raspuns, "nu\n") == 0) // trebuie sa compar cu "nu\n" deoarece din cauza ca in buffer ramane newline
    {
      char *stop_adaugare_comori = "Incheiere adaugare comori.\n";
      write(1, stop_adaugare_comori, strlen(stop_adaugare_comori));
      break;
    }

    char *ID_comoara = "ID comoara: ";
    write(1, ID_comoara, strlen(ID_comoara));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, sizeof(buffer));
    treasure.id = atoi(buffer); // convertesc sirul de caractere din char in int

    char *nume_utilizator = "Nume utilizator: ";
    write(1, nume_utilizator, strlen(nume_utilizator));
    memset(treasure.user_name, 0, sizeof(treasure.user_name));
    read(0, treasure.user_name, sizeof(treasure.user_name));

    char *latitudine_gps = "Latitudine GPS: ";
    write(1, latitudine_gps, strlen(latitudine_gps));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, sizeof(buffer));
    treasure.GPS_latitude = atof(buffer); // convertesc sirul de caractere din char in float

    char *longitudine_gps = "Longitudine GPS: ";
    write(1, longitudine_gps, strlen(longitudine_gps));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, sizeof(buffer));
    treasure.GPS_longitude = atof(buffer);

    char *indiciu = "Indiciu: ";
    write(1, indiciu, strlen(indiciu));
    memset(treasure.clue, 0, sizeof(treasure.clue));
    read(0, treasure.clue, sizeof(treasure.clue));

    char *valoare_comoara = "Valoare comoara: ";
    write(1, valoare_comoara, strlen(valoare_comoara));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, sizeof(buffer));
    treasure.value = atoi(buffer);

    // Construiesc calea catre fisierul comaoraNRCOMOARA.bin
    char path[256];
    snprintf(path, sizeof(path), "%s/comoara%d.bin", hunt_id, treasure_count);

    // Creez fisierul binar pentru fiecare comoara care va contine datele acesteia
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
      char *eroare_deschidere_fisier = "Eroare la deschiderea fisierului binar!\n";
      write(1, eroare_deschidere_fisier, strlen(eroare_deschidere_fisier));
      return 1;
    }

    fwrite(&treasure, sizeof(Treasure), 1, f);
    fclose(f);

    char *succes_adaugare_comoara = "Comoara a fost adaugata cu succes!\n";
    write(1, succes_adaugare_comoara, strlen(succes_adaugare_comoara));
    treasure_count++; // cresc contorul si trec la urmatoarea posibila comoara de adaugat

    log_operation(hunt_id, "A fost adaugata o comoara.");
    create_symlink(hunt_id);
  }

  return 0;
}

// Afisez toate datele, inclusiv dimensiunea fisierului si data ultimei modificari a comorilor dintr-un fisier hunt specificat
// LAB 7
void list_hunt_treasures(const char *hunt_id)
{
  DIR *dir = opendir(hunt_id);
  if (dir == NULL)
  {
    char *eroare_deschidere_director = "Eroare la deschiderea directorului hunt!\n";
    write(1, eroare_deschidere_director, strlen(eroare_deschidere_director));
    exit(-1);
  }

  struct dirent *intrare; // struct standard pentru functia readdir
  struct stat st;         // struct standard pentru atributele unui fisier
  char path[256];         // calea catre un fisier cu terminatia .bin
  // int gasit = 1;          // variabila pentru a verifica daca am gasit un fisier de tip comoara
  long total_size = 0;    // dimensiunea totala a fisierelor
  time_t latest_time = 0; // data ultimei modificari
  int k = 0;              // contor pentru a verifica cate comori am gasit

  while ((intrare = readdir(dir)) != NULL)
  {
    if ((strncmp(intrare->d_name, "comoara", 7) == 0) && (strstr(intrare->d_name, ".bin"))) // verific ca fisierul sa fie unul de tip: comoara[...].bin
    // verific pt fix 7 deoarece altfel nu ia in considerare numarul comorii
    {
      k++; // cresc contorul de comori
      // path = hunt_id + "/" + intrare->d_name
      memset(path, 0, sizeof(path)); // golesc bufferul
      strcpy(path, hunt_id);         // copiez numele hunt-ului
      strcat(path, "/");
      strcat(path, intrare->d_name); // adaug numele fisierului cu terminatie .bin
      // Verific daca fisierul exista
      if (stat(path, &st) != 0)
      {
        char *eroare_existenta_fisier = "Eroare la obtinerea atributelor fisierului!\n";
        write(1, eroare_existenta_fisier, strlen(eroare_existenta_fisier));
        continue;
      }
      if (stat(path, &st) == 0) // daca fisierul exista si poate fi accesat
      {
        total_size = total_size + st.st_size; // st.st_size contine dimensiunea fisierului
        if (st.st_mtime > latest_time)        // faca fisierul actual are o data mai recenta decat ce am salvat pana acum
        {
          latest_time = st.st_mtime; // actualizez latest time
        }

        // if (gasit == 1)
        //{
        //  int k = 1; // contor pentru a verifica cate comori am gasit

        char mesaj0[128];
        char mesaj1[128];
        char mesaj2[128];
        char mesaj3[128];
        char timp_modif[64]; // vector pentru timp

        // mesaj0: "Hunt: <nume_hunt>\n"
        strcpy(mesaj0, "\nHunt: ");
        strcat(mesaj0, hunt_id);
        strcat(mesaj0, "\n");
        write(1, mesaj0, strlen(mesaj0));

        // mesaj1: "Cunt: <nume_comoara>\n"
        char k_char[10];          // vaianta char a contorului de comori
        sprintf(k_char, "%d", k); // convertesc int la string
        strcpy(mesaj1, "Comoara: ");
        strcat(mesaj1, k_char);
        strcat(mesaj1, "\n");
        write(1, mesaj1, strlen(mesaj1));

        // mesaj2: "Dimensiune totala fisiere comoara: <bytes> bytes\n"
        char numar[32];
        // Afisez pe rand dimensiunea fisierului cu o comoara la care se tot adauga comori
        sprintf(numar, "%ld", total_size); // convertesc long la string
        strcpy(mesaj2, "Dimensiune totala fisiere comoara: ");
        strcat(mesaj2, numar);
        strcat(mesaj2, " bytes\n");
        write(1, mesaj2, strlen(mesaj2));

        // Timp: "YYYY-MM-DD HH:MM:SS"
        // https://en.cppreference.com/w/cpp/chrono/c/strftime -> strftime
        strftime(timp_modif, sizeof(timp_modif), "%Y-%m-%d %H:%M:%S", localtime(&latest_time));

        strcpy(mesaj3, "Ultima modificare: ");
        strcat(mesaj3, timp_modif);
        strcat(mesaj3, "\n\n");
        write(1, mesaj3, strlen(mesaj3));

        const char *separator = "-------------------------\n\n";
        write(1, separator, strlen(separator));

        // k=k+existing_treasure_count(hunt_id); // cresc contorul
        // gasit = 0;
        // }
      }
    }
  }

  closedir(dir);
}

// Afisez toate datele din structul unei comori specificate (in functie de ID, dat de la tastatura), din interiorul unui hunt dat ca argument in linie de comanda
// LAB 7
void view_hunt_treasure(const char *hunt_id, int id_comoara)
{
  char path[256];

  // Construiesc calea catre comoaraX.bin
  memset(path, 0, sizeof(path));
  strcpy(path, hunt_id);
  strcat(path, "/comoara");

  char numar[16];
  sprintf(numar, "%d", id_comoara); // convertesc int in string
  strcat(path, numar);
  strcat(path, ".bin");
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    char *eroare_deschidere_fisier = "Eroare la deschiderea fisierului!\n";
    write(1, eroare_deschidere_fisier, strlen(eroare_deschidere_fisier));
    exit(-1);
  }
  else
  {
    Treasure t;
    fread(&t, sizeof(Treasure), 1, f);

    char afisare[256];
    sprintf(afisare, "ID: %d\n", t.id); // convertesc din int in string
    write(1, afisare, strlen(afisare));
    strcpy(afisare, "Utilizator: ");
    strcat(afisare, t.user_name); // t.user_name conține deja \n din read
    write(1, afisare, strlen(afisare));
    sprintf(afisare, "Lat: %.4f, Lon: %.4f\n", t.GPS_latitude, t.GPS_longitude); // convertesc coordonatele din float in string
    write(1, afisare, strlen(afisare));
    strcpy(afisare, "Indiciu: ");
    strcat(afisare, t.clue); // t.clue conține deja \n din read
    write(1, afisare, strlen(afisare));
    sprintf(afisare, "Valoare: %d\n", t.value); // convertesc din int in string
    write(1, afisare, strlen(afisare));
    const char *separator = "\n-------------------------\n\n";
    write(1, separator, strlen(separator));

    fclose(f);
  }
}

// Functie de stergere a unei comori dintr-un hunt
void remove_treasure(const char *hunt_id)
{
  char buffer[16];
  char *mesaj = "Introduceti ID-ul comorii pe care doriti sa o stergeti: ";
  write(1, mesaj, strlen(mesaj));

  memset(buffer, 0, sizeof(buffer));
  read(0, buffer, sizeof(buffer));

  // Curat newline-ul
  char *newline = strchr(buffer, '\n');
  if (newline != NULL)
  {
    *newline = '\0';
  }

  int id_comoara = atoi(buffer);

  // Construiesc calea catre comoaraID.bin
  char path[256];
  memset(path, 0, sizeof(path));
  strcpy(path, hunt_id);
  strcat(path, "/comoara");

  char nr[16];
  sprintf(nr, "%d", id_comoara);
  strcat(path, nr);
  strcat(path, ".bin");

  if (unlink(path) != 0)
  {
    char *eroare_stergere_comoara = "Eroare la stergerea comorii sau comoara nu exista.\n";
    write(1, eroare_stergere_comoara, strlen(eroare_stergere_comoara));
    exit(-1);
  }
  else
  {
    char *succes_stergere_comoara = "Comoara a fost stearsa cu succes.\n";
    write(1, succes_stergere_comoara, strlen(succes_stergere_comoara));
  }

  log_operation(hunt_id, "A fost stearsa o comoara.");
}

// Funtie de stergere a unui hunt
void remove_hunt(const char *hunt_id)
{
  DIR *dir = opendir(hunt_id);
  if (dir == NULL)
  {
    char *eroare_deschidere_director = "Eroare la deschiderea directorului hunt!\n";
    write(1, eroare_deschidere_director, strlen(eroare_deschidere_director));
    exit(-1);
  }

  struct dirent *intrare;
  char path[256];

  while ((intrare = readdir(dir)) != NULL)
  {
    // Ignor "." si ".." ANALOG LAB 7
    if ((strcmp(intrare->d_name, ".") == 0) || (strcmp(intrare->d_name, "..") == 0))
    {
      continue;
    }

    // Construiesc calea completa: hunt_id + "/" + intrare->d_name
    memset(path, 0, sizeof(path));
    strcpy(path, hunt_id);
    strcat(path, "/");
    strcat(path, intrare->d_name);

    // Sterg fisierul
    if (unlink(path) != 0)
    {
      char *eroare_stergere_fisier = "Eroare la stergerea unui fisier din hunt!\n";
      write(1, eroare_stergere_fisier, strlen(eroare_stergere_fisier));
      exit(-1);
    }
  }

  closedir(dir);

  // Dupa ce toate fisierele au fost sterse, stergem directorul
  if (rmdir(hunt_id) != 0)
  {
    char *eroare_stergere_director = "Eroare la stergerea directorului hunt!\n";
    write(1, eroare_stergere_director, strlen(eroare_stergere_director));
    exit(-1);
  }
  else
  {
    char *succes_stergere_hunt = "Hunt-ul a fost sters cu succes.\n";
    write(1, succes_stergere_hunt, strlen(succes_stergere_hunt));

    // Sterg si symlink-ul
    char symlink_path[256];
    snprintf(symlink_path, sizeof(symlink_path), "logged_hunt-%s.txt", hunt_id);
    unlink(symlink_path);
  }

  log_operation(hunt_id, "Hunt-ul a fost sters."); // inainte de rmdir
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    char *eroare_numar_argumente = "Numar gresit de argumente!\n";
    write(1, eroare_numar_argumente, strlen(eroare_numar_argumente));
    exit(-1);
  }

  const char *comanda = argv[1]; // Comanda data ca parametru: add / list / view / remove_treasure / remove_hunt
  const char *hunt_id = argv[2]; // Dat ca parametru

  /* TEST
  if(create_hunt_directory(hunt_id)!=0)
    {
      exit(-1);
    }

  Treasure t;
  memset(&t, 0, sizeof(Treasure)); // Initializez structura cu 0 pentru a scapa de warning
  add_treasure_file(hunt_id, t);
  */

  if (strcmp(comanda, "add") == 0)
  {
    if (create_hunt_directory(hunt_id) != 0)
    {
      exit(-1);
    }
    Treasure t;
    memset(&t, 0, sizeof(Treasure)); // Initializez structura cu 0 pentru a scapa de warning
    add_treasure_file(hunt_id, t);
  }
  else if (strcmp(comanda, "list") == 0)
  {
    list_hunt_treasures(hunt_id);
  }
  else if (strcmp(comanda, "view") == 0)
  {
    char buffer[16];
    char *mesaj_introducere_id_comoara = "Introduceti ID-ul comorii pe care doriti sa o vedeti: ";
    write(1, mesaj_introducere_id_comoara, strlen(mesaj_introducere_id_comoara));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, sizeof(buffer));

    // Curat newline-ul
    char *newline = strchr(buffer, '\n');
    if (newline != NULL)
    {
      *newline = '\0';
    }

    int nr_comoara = atoi(buffer);
    view_hunt_treasure(hunt_id, nr_comoara);
  }
  else if (strcmp(comanda, "remove_treasure") == 0)
  {
    remove_treasure(hunt_id);
  }
  else if (strcmp(comanda, "remove_hunt") == 0)
  {
    remove_hunt(hunt_id);
  }
  else
  {
    char *eroare_comanda = "Comanda necunoscuta!\n";
    write(1, eroare_comanda, strlen(eroare_comanda));
  }

  return 0;
}
