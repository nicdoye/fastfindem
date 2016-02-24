//
//  main.c
//  tidy
//
//  Created by Nicolas Doye on 17/02/2016.
//  Copyright © 2016 Nicolas Doye. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#define VERSION                 0.2

#define _T_MTIME_BASE           10
#define _T_SECONDS_PER_DAY      60 * 60 * 24
#define _T_DOT                  "."
#define _T_DOT_DOT              ".."
#define _T_PATH_SEPARATOR       "/"
/* Saves calculating it every time */
#define _T_PATH_SEPARATOR_LEN   1
#define _T_PERROR_BUF_LEN       80

/* Optional
#define _T_DEBUG
#define _T_DRY_RUN
 */

extern char **environ;

void tidy_dir(int *, time_t, char*);
void tidy_item(int *, time_t , char *, char *, int);
long mtime_long(char*);
time_t then_time(long mtime);

void check_malloc(void *);
void canonicalise_item(char *, char *, int, char **);

int
main(int argc, char *argv[])
{
  int removed = 0;
  char *mtime_str;
  char *path;
  long mtime;
  time_t then;
  
#ifdef _T_DEBUG
  setvbuf(stderr, NULL, _IONBF, 0);
#endif
  
  if (argc != 3) {
    fprintf(stderr, "Need 2 arguments dude. Directory name, and number of days\n");
    exit(EXIT_FAILURE);
  }
  
  path = argv[1];
  mtime_str = argv[2];
  
  /* Calculate the number of days based on input */
  mtime = mtime_long(mtime_str);
  then  = then_time(mtime);
  
  tidy_dir(&removed, then, path);
  
  fprintf(stderr, "%d files deleted\n", removed);
  exit(EXIT_SUCCESS);
}

/*
 * Returns number of items removed
 */
void
tidy_item(int *removed_p, time_t then, char *item_name, char *path, int path_len)
{
  char        *item;
#ifndef _T_DRY_RUN
  char        perror_str[_T_PERROR_BUF_LEN + 1];
#endif
  struct stat *buf;
  int         ret;
  
  if (( strcmp(item_name, _T_DOT) == 0 ) || (strcmp(item_name, _T_DOT_DOT) == 0)) {
    return;
  }
  
  canonicalise_item(item_name, path, path_len, &item);
  
  buf = malloc (sizeof(struct stat));
  check_malloc((void *) buf);
  
  ret = lstat(item, buf);
  if (ret != 0) {
    fprintf(stderr, "can't stat '%s' - ignoring\n", item);
    free(item);
    free(buf);
    return;
  }
  
  if (S_ISDIR(buf->st_mode)) {
    tidy_dir(removed_p, then, item);
    rmdir(item);
  } else if (S_ISREG(buf->st_mode) && ( difftime(then, buf->st_mtime) >0 )) {
    
#ifdef _T_DRY_RUN
    fprintf(stderr,"REMOVE %s\n", item);
    ++(*removed_p);
#else
    
    if (unlink(item) == 0) {
      ++(*removed_p);
    } else {
      snprintf(perror_str, (size_t)_T_PERROR_BUF_LEN, "Couldn't remove %s\n", item);
      perror(perror_str);
    }
#endif
  }
  
  free(item);
  free(buf);
}

/* Let's loop */
void
tidy_dir(int *removed_p, time_t then, char *path)
{
  int ret;
  char perror_str[_T_PERROR_BUF_LEN + 1];
  struct dirent *dirent;
  DIR *dir;
  
  /* Open the directory */
  dir = opendir(path);
  
  if ( NULL == dir ) {
    snprintf(perror_str, (size_t)_T_PERROR_BUF_LEN, "Can't open dir %s - ignoring\n", path);
    perror(perror_str);
    return;
  }
  
  /* Only calculate path_len once per folder */
  int path_len = (int)strlen(path);
  
  /*
   * Actual looping across directory part
   */
  while (NULL != (dirent = readdir(dir))) {
    tidy_item(removed_p, then, dirent->d_name, path, path_len);
  }
  
  /* close directory */
  ret = closedir(dir);
  
  if ( ret != 0 ) {
    snprintf(perror_str, (size_t)_T_PERROR_BUF_LEN, "Failed to close dir %s - ignoring\n", path);
    perror(perror_str);
  }
}

/*
 * General Utility functions
 */

/*
 * No point carrying on in all cases
 */
void
check_malloc(void * p)
{
  if (NULL == p) {
    fprintf(stderr, "Out of memory. Bailing\n");
    exit(EXIT_FAILURE);
  }
}

/*
 * Given a number of days (mtime)
 * return the time_t of this exact time minus that number of days
 */
time_t
then_time(long mtime)
{
  time_t        now;
  
  /* Convert it to seconds */
  mtime *= _T_SECONDS_PER_DAY;
  
  if (mtime > INT_MAX) {
    fprintf(stderr, "Whoa cowboy %ld is a lot of seconds. Baiing\n", mtime);
    exit(EXIT_FAILURE);
  }
  
  if ( time(&now) == (time_t)(-1) ){
    fprintf(stderr, "Mad time error. Sorry\n");
    exit(EXIT_FAILURE);
  }
  
  /* we can cast this, becuase we checked above */
  return(now - (int) mtime);
}

/*
 * Given a time string, turn it into a long
 * This code is pretty much the example given in man strtol(3)
 */
long
mtime_long(char* mtime_str)
{
  char    *endptr;
  char    perror_str[_T_PERROR_BUF_LEN + 1];
  
  /* Convert the number in mtime_str into a long. Place in mtime */
  long    mtime = strtol(mtime_str, &endptr, _T_MTIME_BASE);
  
  /* Check for various possible errors */
  
  if ((errno == ERANGE && (mtime == LONG_MAX || mtime == LONG_MIN))
      || (errno != 0 && mtime == 0)) {
    snprintf(perror_str, (size_t)_T_PERROR_BUF_LEN, "couldn't convert %s to a long\n", mtime_str);
    perror(perror_str);
    exit(EXIT_FAILURE);
  }
  
  if (endptr == mtime_str) {
    fprintf(stderr, "No digits were found\n");
    exit(EXIT_FAILURE);
  }
  
  if (*endptr != '\0') {
    fprintf(stderr, "Further characters after number: %s\n", endptr);
    exit(EXIT_FAILURE);
  }
  
  /* Absolute value. Don't mess with me here */
  return(labs(mtime));
}

/*
 * Convert path/item_name to canonical name and place in item.
 */
void
canonicalise_item(char *item_name, char *path, int path_len, char **item_p)
{
  int     newsize;
  char    *non_canonicalised_item;
  
  unsigned long item_name_len = strlen(item_name);
  newsize = (int)(path_len + _T_PATH_SEPARATOR_LEN + item_name_len + 1);
  
  non_canonicalised_item = malloc(sizeof(char) * (newsize)); /*  calloc(newsize + 1, sizeof(char)); */
  check_malloc((void *) non_canonicalised_item);
  
  /*
   * Given we've just allocated the correct amount of data
   * and that strcpy is meant to be faster than strncpy etc.
   * I'm using the good, old-fashioned versions of strcpy and strcat
   */
  
  strcpy (non_canonicalised_item, path);
  strcat (non_canonicalised_item, _T_PATH_SEPARATOR);
  strcat (non_canonicalised_item, item_name);
  
  *item_p = strdup(non_canonicalised_item);
  
  free(non_canonicalised_item);
}

