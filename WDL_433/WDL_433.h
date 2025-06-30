/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*
    WDL_433.h
    Includes and definitions used in WDL_433, weather data logger
    HDTodd@gmail.com, 2025.04.23
*/

#pragma once

#include <time.h>
#include <linux/limits.h>

#define APP_NAME    "WDL_433"
#define APP_VERSION "v1.1"

// Database and table names used by sqlite3 & MariaDB/MySQL
#define DBPATH  "/var/databases/"
#define DBNAME  "Weather"
#define DBTABLE "SensorData"

// max time difference, in sec, for two records from same sensor not to be duplicates
#define DUP_REC 2  
// minimum time between archived database records for each sensor, in sec
#define recordingInterval 5*60   

#ifndef USE_SQLITE3
#ifdef USE_MYSQL
  #include <mariadb/mysql.h>
#else
  #define USE_SQLITE3
  #include <sqlite3.h>
#endif // end ifdef USE_MYSQL
#endif // end ifndef USE_SQLITE3

// File-handling definitions
#define FNLEN      NAME_MAX
#define INI_PATH   ".:~:/usr/local/etc:/etc"      // search path for .ini & aliases
#define INI_FILE   APP_NAME".ini"                 // default name of .ini file

typedef enum {HTTP, MQTT} source_t;               // future HTTP streaming option

// This is the structure to store data for database records
typedef struct {
    char    date_time[20];;
    char    sensorID[50];
    double  temp1;
    double  temp2;
    double  rh;
    double  press;
    double  light;
} DBRecord;


// We need the binary-tree node structure for procedures below
typedef struct node {
    char          *key;
    char          *alias;
    time_t         lasttime;
    struct node   *lptr;
    struct node   *rptr;
} NODE, *NPTR;

//  We need the cmdlist_t definitions for the handlers below
#include "GetSetParams.h"

// General utility procedures
void intHandler(int sigType);
void strLower(char* s);
bool isnumeric(char *str);
void PrintParams(cmdlist_t *cmdlist, char *header);
NPTR node_find(NPTR p, char *key, bool Create);
void tree_print(NPTR p);


// .ini and CLI setters
void setDebug(void);
void setGDebug(void);
void helper(cmdlist_t *cmdlist);
void setSource(char *optarg);
void setHost(char *optarg);
void setPort(char *optarg);
void setTopic(char *optarg);

// SQL processing procedures
void appendToDB(DBRecord *DBRow);
void initDBMgr(void);
#ifdef USE_SQLITE3
void setSql3file(char *optarg);
void setSql3path(char *optarg);
#else
void setMyHost(char *optarg);
void setMyUser(char *optarg);
void setMyPass(char *optarg);
#endif

