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

#define _T_MTIME_BASE           10
#define _T_SECONDS_PER_DAY      60 * 60 * 24
#define _T_DOT                  "."
#define _T_DOT_DOT              ".."
#define _T_PATH_SEPARATOR       "/"
/* Saves calculating it every time */
#define _T_PATH_SEPARATOR_LEN   1
#define _T_PERROR_BUF_LEN       80

/* Optionals
 #define _T_DEBUG
 #define _T_DRY_RUN
 */

/*
 * Linked List - not the fastest O(n^2)!, but q+d to implement
 *
 * If we traverse n directories (including the root directory)
 * There are sum (1 .. (n -1)) checks = (n - 1)(n/2) = (n^2 - n)/2
 * For 30 folders that's 435 comparisons. Gulp.
 *
 * We cache path_len to stop future strlen()s
 */
struct node {
  char            *path;
  unsigned long   path_len;
  struct node     *next;
};


extern char **environ;

void tidy_dir(int *, time_t, char*, struct node **);
void tidy_item(int *, time_t, char *, struct node *, struct node **);
long mtime_long(char*);
time_t then_time(long mtime);

bool add_node(struct node *, struct node *);
void node_init(struct node **, char *);
void check_malloc(void *);
void free_node(struct node **);
void free_nodes(struct node *);
void canonicalise_item(char *, struct node *, char **);

int
main(int argc, char *argv[])
{
  int removed = 0;
  char *mtime_str;
  char *path;
  long mtime;
  time_t then;
  struct node **root_node_p;
  
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
  
  root_node_p = malloc(sizeof(struct node *));
  check_malloc((void *) root_node_p);
  *root_node_p = NULL;
  
  tidy_dir(&removed, then, path, root_node_p);
  
  free_nodes(*root_node_p);
  free(root_node_p);
  
  fprintf(stderr, "%d files deleted\n", removed);
  exit(EXIT_SUCCESS);
}

/*
 * Returns number of items removed
 */
void
tidy_item(int *removed_p, time_t then, char *item_name, struct node *this_node, struct node **root_node_p)
{
  char        *item;
  char        perror_str[_T_PERROR_BUF_LEN + 1];
  struct stat *buf;
  int         ret;
  
  if (( strcmp(item_name, _T_DOT) == 0 ) || (strcmp(item_name, _T_DOT_DOT) == 0)) {
    return;
  }
  
  canonicalise_item(item_name, this_node, &item);
  
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
    tidy_dir(removed_p, then, item, root_node_p);
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
  
  free(buf);
  free(item);
}

/* Let's loop */
void
tidy_dir(int *removed_p, time_t then, char *non_canonicalised_path, struct node **root_node_p)
{
  int ret;
  char path[PATH_MAX + 1];
  char perror_str[_T_PERROR_BUF_LEN + 1];
  bool new_folder = true;
  struct node *this_node;
  struct dirent *dirent;
  DIR *dir;
  
  /* Get canonical path */
  realpath(non_canonicalised_path, path);
  
  this_node = malloc(sizeof(struct node));
  check_malloc((void *) this_node);
  node_init(&this_node, path);
  
  if (*root_node_p == NULL) {
    /* First pass root is null */
    *root_node_p = this_node;
  } else {
    /* New Node to be added */
    new_folder = add_node(*root_node_p, this_node);
    
    if ( !new_folder ) {
      fprintf(stderr, "Been here before. ignoring %s\n", path);
      free_node(&this_node);
      return;
    }
  }
  
  /* Open the directory */
  dir = opendir(path);
  
  if ( NULL == dir ) {
    snprintf(perror_str, (size_t)_T_PERROR_BUF_LEN, "Can't open dir %s - ignoring\n", path);
    perror(perror_str);
    return;
  }
  
  /*
   * Actual looping across directory part
   */
  while (NULL != (dirent = readdir(dir))) {
    tidy_item(removed_p, then, dirent->d_name, this_node, root_node_p);
  }
  
  /* close directory */
  ret = closedir(dir);
  
  if ( ret != 0 ) {
    snprintf(perror_str, (size_t)_T_PERROR_BUF_LEN, "Failed to close dir %s - ignoring\n", path);
    perror(perror_str);
  }
}

/*
 * Walk to the end and add the new_node.
 * Returns true iff it appends the node.
 * Returns false if it found the path already.
 */
bool
add_node(struct node *root_node, struct node *new_node)
{
  struct node *this_node = NULL;
  struct node *head_node;
  
  if (NULL == root_node) {
    fprintf(stderr, "Can't walk along an empty list. Returning false, but could be broken\n");
    return(false);
  }
  
  /* Check if this path has already been added */
  
  head_node = root_node;
  while (NULL != head_node) {
    this_node = head_node;
    head_node = head_node->next;
    
    /* path_len check may help, but if you have a folder */
    
    if ( this_node->path_len == new_node->path_len &&
        strcmp(this_node->path, new_node->path) == 0) {
      /*
       * The new node matches the current iterator node.
       * containing e.g. timestamp folders it's an extra
       * (although quick) test
       */
      return(false);
    }
  }
  if (NULL == this_node) {
    /* This is purely here to stop Xcode moaning */
    fprintf(stderr, "this_node is NULL. Bailing\n");
    
    return false;
  }
  this_node->next = new_node;
  
  return(true);
}

/*
 * Walk along the list. Freeing them all
 */
void
free_nodes(struct node *root_node)
{
  struct node *this_node;
  struct node *head_node;
  
  if (NULL == root_node) {
    fprintf(stderr, "Can't walk along an empty list. Returning false, but could be broken\n");
    return;
  }
  
  head_node = root_node;
  while (NULL != head_node) {
    this_node = head_node;
    head_node = head_node->next;
    free_node(&this_node);
  }
}

void
free_node(struct node **node)
{
  free((*node)->path);
  free(*node);
}

/*
 * Set up values for node
 */
void
node_init(struct node **node_p, char *path)
{
  if (NULL == node_p) {
    fprintf(stderr, "Nonsensical for node_p to be NULL. Bailing\n");
    exit(EXIT_FAILURE);
  }
  
  (*node_p)->path = strdup(path);
  (*node_p)->path_len = strlen(path);
  (*node_p)->next = NULL;
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
canonicalise_item(char *item_name, struct node *this_node, char **item_p)
{
  int     newsize;
  char    *non_canonicalised_item;
  char    item[PATH_MAX + 1];
  
  unsigned long item_name_len = strlen(item_name);
  newsize = (int)(this_node->path_len + _T_PATH_SEPARATOR_LEN + item_name_len + 1);
  
  non_canonicalised_item = malloc(sizeof(char) * (newsize)); /*  calloc(newsize + 1, sizeof(char)); */
  check_malloc((void *) non_canonicalised_item);
  
  /*
   * Given we've just allocated the correct amount of data
   * and that strcpy is meant to be faster than strncpy etc.
   * I'm using the good, old-fashioned versions of strcpy and strcat
   */
  
  strcpy (non_canonicalised_item, this_node->path);
  strcat (non_canonicalised_item, _T_PATH_SEPARATOR);
  strcat (non_canonicalised_item, item_name);
  
  realpath(non_canonicalised_item, item);
  *item_p = strdup(item);
  
  free(non_canonicalised_item);
}

