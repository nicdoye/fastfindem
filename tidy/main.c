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

#define TIDY_MTIME_BASE      10
#define TIDY_SECONDS_PER_DAY 60 * 60 * 24
#define TIDY_HERE_DIR        "."
#define TIDY_PARENT_DIR      ".."
#define TIDY_PATH_SEPARATOR  "/"
#define TIDY_PERROR_BUF_LEN  80
#define TIDY_DEBUG

/* Linked List - not the fastest, but q+d to implement*/
struct node {
    char        *path;
    struct node *next;
};


extern char **environ;

int loopy(time_t, char*, struct node*, struct node*, struct node**);
long mtime_long(char*);
bool path_already_read(struct node *, char *);
time_t then_time(long mtime);

void new_node_empty(struct node **);
void new_node(struct node **, char *);
void add_node(struct node **, struct node *, char *);

int main
(int argc, char *argv[])
{
    int total;
    char *mtime_str;
    char *path;
    long mtime;
    time_t then;
    struct node **last;
    
#ifdef TIDY_DEBUG
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
    last = malloc(sizeof(struct node *));
    
    total = loopy(then, path, NULL, NULL, last);
    
    fprintf(stderr, "%d files deleted\n", total);
    exit(EXIT_SUCCESS);
}

time_t
then_time(long mtime)
{
    time_t        now;
    
    /* Convert it to seconds */
    mtime *= TIDY_SECONDS_PER_DAY;
    
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

long
mtime_long(char* mtime_str)
{
    char    *endptr,
            perror_str[TIDY_PERROR_BUF_LEN + 1];
    long    mtime = strtol(mtime_str, &endptr, TIDY_MTIME_BASE);
    
    /* Check for various possible errors */
    
    if ((errno == ERANGE && (mtime == LONG_MAX || mtime == LONG_MIN))
        || (errno != 0 && mtime == 0)) {
        snprintf(perror_str, (size_t)TIDY_PERROR_BUF_LEN, "couldn't convert %s to a long\n", mtime_str);
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

/* Let's loop */
int
loopy(time_t then, char *unfiltered_path, struct node *root, struct node *current, struct node **last)
{
    /* removed is the number of files & directories deleted */
    int removed = 0;
    int ret;
    int newsize;
    int nuked = 0;
    char path[PATH_MAX + 1];
    char perror_str[TIDY_PERROR_BUF_LEN + 1];
    char *unflitered_item;
    char *unfiltered_item2;
    char *item;
    struct node *node;
    struct dirent *dirent;
    struct stat *buf;
    DIR *dir;
    
    realpath(unfiltered_path, path);

    if (root == NULL) {
        /* No Root - must be the first try */

        new_node(&node, path);
        root = node;
        current = root;
    } else {
        if (current == NULL) {
            fprintf(stderr, "This can't be right I have a root, but no current node %s\n", path);
            return( removed );
        }
        if (path_already_read(root, path)) {
            fprintf(stderr, "Been here before. ignoring %s\n", path);
            return( removed );
        }
        
        /* always add the path into visited - doesn't matter if it's valid or perms issues */
        add_node(&node, current, path);
        current = node;
    }
    
    if (last == NULL) {
        fprintf(stderr, "Nonsensical for last not to be set. Bailing\n");
        exit(EXIT_FAILURE);
    }

    /* current is at the end, so set *last to it */
    
    *last = current;
    
    /* open folder */

    dir = opendir(path);
    
    if ( dir == NULL ) {
        snprintf(perror_str, (size_t)TIDY_PERROR_BUF_LEN, "Can't open dir %s - ignoring\n", path);
        perror(perror_str);
        return(0);
    }

    while ((dirent = readdir(dir)) != NULL) {
        if (( strcmp(dirent->d_name, TIDY_HERE_DIR) == 0 ) || (strcmp(dirent->d_name, TIDY_PARENT_DIR) == 0)) {
            continue;
        }
        fprintf(stderr, "dirent: %s\n", dirent->d_name);

        newsize = (int)(strlen(path) + strlen(TIDY_PATH_SEPARATOR) + strlen(dirent->d_name));
        fprintf(stderr, "%d, %lu, %lu, %lu\n", newsize, strlen(path), strlen(TIDY_PATH_SEPARATOR) ,strlen(dirent->d_name)  );

        unflitered_item = calloc(newsize + 1, sizeof(char));
        unfiltered_item2 = calloc(newsize + 1, sizeof(char));
        /* Lazy. strncpy/strncat and friends bust my chops */
         
        snprintf(unfiltered_item2, newsize + 1,  "%s%s%s",path, TIDY_PATH_SEPARATOR,dirent->d_name);
        
        /*
         strncpy (newpath, path, strlen(path));
         strncat (newpath, TIDY_PATH_SEPARATOR, (strlen(TIDY_PATH_SEPARATOR)));
         strncat (newpath, dirent->d_name, strlen(dirent->d_name));
         fprintf(stderr, "newpath:  %s\n", newpath);
         */
        fprintf(stderr, "newpath2: %s\n", unfiltered_item2);
        
        item = realpath(unfiltered_item2, NULL);
        
        buf = malloc (sizeof(struct stat));

        ret = lstat(item, buf);
        if (ret != 0) {
            snprintf(perror_str, (size_t)TIDY_PERROR_BUF_LEN, "can't stat %s - ignoring\n", dirent->d_name);
            perror(perror_str);
            free(buf);
            continue;
        }
        
        if (newsize - 1 >= PATH_MAX) {
            fprintf(stderr, "Path length too long: %d - ignoring\n", newsize);
            free(buf);
            continue;
        }
        
        if (S_ISDIR(buf->st_mode)) {

            fprintf(stderr, "Looping with item: %s\n", item);
            fprintf(stderr, "pre  loop:\t%s\tlast:\t%s\n", current->path, (*last)->path);
            
            nuked += loopy(then, item, root, current, last);
            
            fprintf(stderr, "post loop:\t%s\tlast:\t%s\n", current->path, (*last)->path);
            
            /* Attempt to remove it. It may be empty. Ignore errors. Don't care */
            rmdir(item);
        } else if (S_ISREG(buf->st_mode) && ( difftime(then, buf->st_mtime) >0 )) {
            /* NUKE IT */
            fprintf(stderr,"REMOVE %s\n", item);
            /*
             if (unlink(resolvedname) == 0) {
             ++nuked;
             } else {
             snprintf(perror_str, (size_t)TIDY_PERROR_BUF_LEN, "Couldn't remove %s\n", resolvedname);
             perror(perror_str);
             }
             */
        }
        free(buf);
    }
    ret = closedir(dir);
    if ( ret != 0 ) {
        snprintf(perror_str, (size_t)TIDY_PERROR_BUF_LEN, "Failed to close dir %s - ignoring\n", path);
        perror(perror_str);
    }
    
    return nuked;
}

/* Returns added node */
void
add_node(struct node **node_ptr, struct node *current, char *path)
{
    struct node *node;
    
    if (current->next != NULL) {
        fprintf(stderr, "Adding a new node, but wasn't at the end\n");
        exit(EXIT_FAILURE);
    }
    
    new_node(&node, path);
    *node_ptr = node;
    current->next = *node_ptr;
}

void
new_node(struct node **node_ptr, char *path)
{
    struct node *node;
    node = malloc(sizeof(struct node));
    
    new_node_empty(&node);
    *node_ptr = node;
    (*node_ptr)->path = path;
}

void
new_node_empty(struct node **node_ptr)
{
    if (node_ptr == NULL) {
        fprintf(stderr, "Nonsensical for node_ptr to be NULL. Bailing\n");
        exit(EXIT_FAILURE);
    }
    
    if ( *node_ptr == NULL ) {
        (*node_ptr) = malloc(sizeof(struct node));
    }
    
    if ( *node_ptr == NULL ) {
        fprintf(stderr, "Couldn't get memory for new node\n");
        exit(EXIT_FAILURE);
    }
    
    (*node_ptr)->next  = NULL;
}

bool
path_already_read(struct node *root, char *path)
{
    struct node *node;
    
    /* start at root of list and walk along it */
    node = root;
    
    while (node != NULL) {
        /* fprintf(stderr, "node: %s\n", node->path); */
        if (strcmp(node->path, path) == 0) {
            return(true);
        }
        node = node->next;
    }
    return(false);
}
