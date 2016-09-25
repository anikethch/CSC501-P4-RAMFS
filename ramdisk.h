#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

typedef struct node {
  int type;
  mode_t mode;
  char *name;
  char *data;
  int  size;
  struct node *next;
  time_t atime;
  time_t ctime;
  time_t mtime;
}node;
