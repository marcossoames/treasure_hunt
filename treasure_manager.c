#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#define MAX_USER_NAME 50
#define MAX_CLUE 50

// Treasure data
typedef struct {
  int id;
  char user_name[MAX_USER_NAME];
  float GPS_latitude;
  float GPS_longitude;
  char clue[MAX_CLUE];
  int value;
} Treasure;

// Creez un nou director pentru fiecare hunt
int create_hunt_directory(const char *hunt_id)
{

  if((hunt_id==NULL)||(strlen(hunt_id)==0))
    {
      char *invalid="Hunt id este invalid!\n";
      write(1,"Hunt id invalid!\n",strlen(invalid));
      return 1;
    }

  // Verific daca directorul exista deja
  struct stat ab;
  if(stat(hunt_id,&ab)==0)
    {
      char *director_exista="Directorul exista deja!\n";
      write(1,director_exista,strlen(director_exista));
      return 1;
    }

  // Creez directorul cu permisiuni depline
  if(mkdir(hunt_id,0755)!=0)
    {
      char *eroare_creare_director="Eroare la crearea directorului!\n";
      write(1,eroare_creare_director,strlen(eroare_creare_director));
      return 1;
    }

  char *succes_creare_director="Directorul a fost creat cu succes!\n";
  write(1,succes_creare_director,strlen(succes_creare_director));
  return 0;
}

int main(int argc, char *argv[])
{
  if(argc!=3)
    {
      char *eroare_numar_argumente="Numar gresit de argumente!";
      write(1,eroare_numar_argumente,strlen(eroare_numar_argumente));
      exit(-1);
    }

  const char *hunt_id=argv[2]; // Dat ca parametru

  if(create_hunt_directory(hunt_id)!=0)
    {
      exit(-1);
    }

 
  return 0;
}
