/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*  WDL_procs.c
    WDL_433 parameter-setting procedures for use with the
    weather data logger for rtl_433
    
    HDTodd@gmail.com, 2025.04.23
*/

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "WDL_433.h"
#include "GetSetParams.h"

extern bool     DEBUG;
extern bool     GDEBUG;
extern char    *senAlias;
extern source_t source;
extern char    *host;
extern int      port;
extern char    *topic;
#ifdef USE_SQLITE3
extern char    *sql3path;
extern char    *sql3file;
#else
extern char    *myHost;
extern char    *myUser;
extern char    *myPass;
#endif
bool isnumeric(char *str) {
    while (*str!=0) if (!isdigit(*str++)) return(false);
    return(true);
};

// Convert string to lower case
void strLower(char* s) {
  for(char *p=s; *p; p++) *p=tolower(*p);
}

void setDebug(void) {
    DEBUG = true;
};

void setGDebug(void) {
    GDEBUG = true;
};

void helper(cmdlist_t *cmdlist) {
    printf("%s %s: Log weather data from rtl_433 to an SQL database\n",
            APP_NAME, APP_VERSION);
    printf("    Usage: %s [OPTIONS]\n", APP_NAME);
    printf("    [OPTIONS] are any combination of\n    Long form       Short    Option invoked\n");
    for (int i = 0; cmdlist->long_opt[i].name != NULL; i++) {
        printf("    --%-15s-%c      %s\n", cmdlist->long_opt[i].name,
               cmdlist->optaux[i].ltr, cmdlist->optaux[i].desc);
    };
    exit(0);
};

void setSenAlias(char *optarg) {
    char *alias;
    if ( (alias=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(alias, optarg);
    senAlias = alias;
    return;
};

void setSource(char *optarg) {
    strLower(optarg);
    if (strcmp(optarg, "mqtt")==0) source = MQTT;
    else if
        (strcmp(optarg, "http")==0) source = HTTP;
    else {
        fprintf(stderr, "Invalid source protocol %s specified for '--source' option\n", optarg);
        exit(1);
    };
};

void setHost(char *optarg) {
    char *newHost;
    if ( (newHost=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newHost, optarg);
    host = newHost;
    return;
};

void setPort(char *optarg) {
    if (!isnumeric(optarg)) {
        fprintf(stderr, "--port option '%s' is not a number\n", optarg);
        exit(1);
    };
    port = atoi(optarg);
    return;
};

void setTopic(char *optarg) {
    char *newTopic;
    if ( (newTopic=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newTopic, optarg);
    topic = newTopic;
    return;
};

#ifdef USE_SQLITE3
void setSql3path(char *optarg) {
    char *newPath;
    if ( (newPath=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newPath, optarg);
    sql3path = newPath;
    return;
};

void setSql3file(char *optarg) {
    char *newFile;
    if ( (newFile=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newFile, optarg);
    sql3file = newFile;
    return;
};

#else
void setMyHost(char *optarg) {
    char *newHost;
    if ( (newHost=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newHost, optarg);
    myHost = newHost;
    return;
};

void setMyUser(char *optarg) {
    char *newUser;
    if ( (newUser=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "'setMyUser' unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newUser, optarg);
    myUser = newUser;
    return;
};

void setMyPass(char *optarg) {
    char *newPass;
    if ( (newPass=malloc(strlen(optarg)) ) == NULL ) {
        fprintf(stderr, "Unable to allocate memory for option '%s' string\n", optarg);
        exit(1);
    };
    strcpy(newPass, optarg);
    myPass = newPass;
    return;
};
#endif

void PrintParams(cmdlist_t *cmdlist, char *header) {
    printf("\n%s\n", header);
    printf("DEBUG    = %s\n", DEBUG ? "true" : "false");
    printf("GDEBUG   = %s\n", GDEBUG ? "true" : "false");
    printf("senAlias = %s\n", senAlias);
    printf("source   = %s\n", source ? "MQTT" : "HTTP");
    printf("host     = %s\n", host);
    printf("port     = %d\n", port);
    printf("topic    = %s\n", topic);
#ifdef USE_SQLITE3
    printf("sql3path = %s\n", sql3path);
    printf("sql3file = %s\n", sql3file);
#else
    printf("myHost   = %s\n", myHost);
    printf("myUser   = %s\n", myUser);
    printf("myPass   = %s\n", myPass);
#endif
    printf("\n");
    return;
};

/* Simple binary tree keyword search/insert procedures
   Adapted for use in WDL_433 from https://github.com/hdtodd/KeySearch

   This procedure creates and processes a binary tree for
     storing and retrieving keywords and data in lexical order.
   The tree nodes have
     -  a "key" by which the tree is sorted as it is create
     -  attributes: a date-time stamp (Unixepoch format) and
        possibly a pointer to an alias for the keyword
     -  left- and right-subtree pointers

   In general, neither the main procedures nor the tree procedure
   need to know anything about the information stored in
   the attribute nodes.  Attribute nodes are referenced by void * pointers.

   To find the node of a "key" in the tree and optionally add it if it's not there:
       p = node_find(root, key, create)
     where "key" is a (char *) string and "root" is the root of the tree
     "node_find" returns the pointer to the node:
     -  either a pointer to a previously-created node or (if 'create' = true)
        a pointer to a newly-created node with the keyword embedded in it,
        its 'alias' pointer set to NULL, and the timestamp set to 0
     -  NULL if the keyword isn't in the tree and 'create' is false
     In creating a new node to add a new key to the tree,
     "node_find()" COPIES the key into a dynamically-allocated string
     assigned to that node: the calling routine can destroy or
     reuse its copy of the key

   The initial call to node_find() can use a NULL root,
   but the returned pointer should be saved as the base
   of the tree for subsequent calls.

   To print the tree node information (key, alias),
     tree_print(root)

  hdtodd@gmail.com, 2022.05.22; modified for WDL_433 2025.04.16
*/

//  create a new binary tree node and initialize its data values (lastTime, alias)
NPTR node_new(char *key) {
    NPTR p;
    if (DEBUG) printf("Entering node_new with key '%s' to create node\n", key);
    p = (NPTR) malloc(sizeof(NODE));
    if (p == NULL) {
        fprintf(stderr, "Out of memory allocating space for a new binary tree node in 'bintree'\n");
        exit(EXIT_FAILURE);
    };
    p->key = (char *) malloc(strlen(key)+1);
    strcpy(p->key,key);
    p->alias    = NULL;
    p->lasttime = 0x00000000;
    p->lptr     = p->rptr    = NULL;
    return p;
};

// Find the tree node assocated with 'key' and optionally create a new node
//   at the appropriate place in the binary tree if not found
NPTR node_find(NPTR root, char *key, bool create) {
  int cond;
  NPTR p = root;;
  if (p == NULL) {
      if (create)
          return(node_new(key));
      else
          return NULL;
  };
  while (p != NULL) {
    if ( (cond=strcmp(key, p->key)) == 0)
      return(p);
    else if (cond<0)
      if (p->lptr == NULL) return (p->lptr = node_new(key));
      else p = p->lptr;
    else
      if (p->rptr == NULL) return (p->rptr = node_new(key));
      else p = p->rptr;
  };
  return(NULL);  // should never get here, but just in case
};

// In-order printing of the tree 
void tree_print(NPTR p) {
  if (p != NULL) {
    tree_print(p->lptr);
    // Print node information, then attribute information
    printf("    sensorID %-20s", p->key);
    if (p->alias == NULL)
        printf("\n");
    else
        printf(" aliased as '%s'\n", p->alias);
    tree_print(p->rptr);
  };
};

