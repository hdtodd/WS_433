/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*  WDL-DBMgr.c
    Procedures to open SQL database and toappend  meterological data to
    a MySQL or sqlite3 database.

    Written by HDTodd, hdtodd@gmail.com, 2016, for use with WeatherStation.c
    Revised 2025.04.15 for use with WDL_433, weather data logger for rtl_433
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "WDL_433.h"

extern bool DEBUG;
#define sqlStringLen 300
char sqlString[sqlStringLen];

#ifdef USE_SQLITE3
#include <sqlite3.h>
extern char *sql3path;
extern char *sql3file;
char sql3fullpath[FNLEN+1];
sqlite3 *db;          // database handle
char    *zErrMsg = 0, // returned error code
        *sql,         // sql command string
         outstr[200]; // space for datetime string
int rc;               // result code from sqlite3 function calls

static int callback(void *NotUsed, int argc, char **argv,
        char **azColName); // not used at present but ref'd by sqlite3 call
#endif

#ifdef USE_MYSQL
#include <mariadb/mysql.h>
extern char *myHost;                          // server host (default=localhost)
extern char *myUser;                          // username (default=login name)
extern char *myPass;                          // password (default=none)
static unsigned int opt_port_num = 3306;      // port number (use built-in value)
static char *opt_socket_name     = NULL;      // socket name (use built-in value)
static unsigned int opt_flags    = 0;         // connection flags (none)
MYSQL *mysql;                                 // connection handler
MYSQL_RES *res;
MYSQL_ROW row;
#endif

void initDBMgr(void) {
#ifdef USE_SQLITE3
    //create sqlite3 db if necessary
    snprintf(sql3fullpath, FNLEN, "%s/%s", sql3path, sql3file);
    if (DEBUG) printf("Initializing sqlite3 database at '%s' if necessary\n",
                      sql3fullpath);
    rc = sqlite3_open(sql3fullpath, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "?Can't open or create sqlite3database %s\n%s\n",
                sql3fullpath, sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    } else {
        if (DEBUG) printf("Opened sqlite3 database %s\n", sql3fullpath);
    };

    // If the table doesn't exist, create it
    strcpy(sqlString, "CREATE TABLE if not exists "); strcat(sqlString, DBTABLE);
    strcat(sqlString, " (date_time TEXT, sensorID TEXT,");
    strcat(sqlString, "temp1 REAL, temp2 REAL, rh REAL, ");
    strcat(sqlString, "press REAL, light REAL)");
    if (DEBUG) printf("Creating sqlite3 database table with command\n   %s\n", sqlString);
    rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "?Can't open or create sqlite3 database table '%s'\n", DBTABLE);
        fprintf(stderr, "\tsqlite3 error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }
    else {
        if (DEBUG) printf("sqlite3 table '%s' opened or created successfully\n", DBTABLE);
    };
    sqlite3_close(db);
    return;
#endif

#ifdef USE_MYSQL
    if (DEBUG) printf("Check for and connect to MySQL database\n");
    // First, init the MySQL handle
    if (!(mysql = mysql_init(0))) {
        fprintf(stderr, "?mysql_init() failed (probably out of memory)\n");
        exit(EXIT_FAILURE);
    };
    if (DEBUG) printf("mysql_init(0) succeeded\nConnect to host '%s'\n", myHost);
    if (mysql_real_connect(mysql, // connect to server
                myHost, myUser, myPass,
                NULL, opt_port_num, opt_socket_name,
                opt_flags) == NULL) {
        fprintf(stderr, "?mysql_real_connect() failed to connect to database\n");
        fprintf(stderr, "\t%s\n", mysql_error(mysql));
        exit(EXIT_FAILURE);
    };
    if (DEBUG) printf("Connected to server '%s'\n", myHost);
    
    // Now, see if the database exists; if not, create it
    if (DEBUG) printf("Checking to see if database %s exists\n", DBNAME);
    if (mysql_select_db(mysql, DBNAME)) {
        snprintf(sqlString, sqlStringLen, "CREATE DATABASE IF NOT EXISTS %s;", DBNAME);
        if (DEBUG) printf("Creating MySQL database with command\n   %s\n", sqlString);
        if (mysql_query(mysql, sqlString) != 0) {  // 
            fprintf(stderr, "?MySQL check for database failed\n");
            fprintf(stderr, "\t%s\n", mysql_error(mysql));
            mysql_close(mysql);
            exit(EXIT_FAILURE);
        };
        if (DEBUG) printf("   ... succeeded\n");
    } else {
        if (DEBUG) printf("Database '%s' already exists\n", DBNAME);
    };

    //  If we get here, the database exists, so use it.
    if (DEBUG) printf("Selecting MySQL database for table access\n");
    if ( mysql_select_db(mysql, DBNAME) != 0 ) {
        fprintf(stderr, "?MySQL: Failed to connect to database '%s'.\n %s\n",
                DBNAME, mysql_error(mysql));
        exit(EXIT_FAILURE);
    };
    
    // If the table doesn't exist, create it
    strcpy(sqlString, "CREATE TABLE IF NOT EXISTS "); strcat(sqlString, DBTABLE);
    strcat(sqlString, " (date_time char(20), sensorID char(50), ");
    strcat(sqlString, "temp1 float, temp2 float, rh float, press float, light float)");
    if (DEBUG) printf("Creating MySQL table with command\n   %s\n", sqlString);
    if (mysql_query(mysql, sqlString) != 0) {
        fprintf(stderr, "?MySQL couldn't create MySQL table with command\n  %s\n", sqlString);
        fprintf(stderr, "\t%s\n", mysql_error(mysql));
        mysql_close(mysql);
        exit(EXIT_FAILURE);
    };

    // Database and table exist and 'mysql' points to it; leave connection open
    return;
#endif
};

void appendToDB(DBRecord *DBRow) {
#ifdef USE_SQLITE3
    /* Open database */
    rc = sqlite3_open(sql3fullpath, &db);
    if (rc) {
        fprintf(stderr, "?Can't open sqlite3 database file '%s'\n%s\n",
                DBNAME, sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    };

    /* Create and execute the INSERT with these data values as parameters*/
    snprintf(sqlString, sizeof(sqlString),
             "INSERT INTO %s (date_time, sensorID, temp1, temp2, rh, press, light) VALUES ('%s', '%s', %5.1f, %5.1f, %3.0f, %6.1f, %3.0f);",
             DBTABLE, DBRow->date_time, DBRow->sensorID, DBRow->temp1, DBRow->temp2, DBRow->rh, DBRow->press, DBRow->light);
    if (DEBUG) printf("sqlite3 insert command:\n    %s\n", sqlString);
    rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "?sqlite3 error during row insert: %s\n", zErrMsg);
        fprintf(stderr, "\tCan't write to database file %s: check permissions\n", DBNAME);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    };
    sqlite3_close(db); // Done with the DB for now so close it
#endif

#ifdef USE_MYSQL
    snprintf(sqlString, sizeof(sqlString),
             "INSERT INTO %s (date_time, sensorID, temp1, temp2, rh, press, light) VALUES ('%s', '%s', %5.1f, %5.1f, %3.0f, %6.1f, %3.0f)",
             DBTABLE, DBRow->date_time, DBRow->sensorID, DBRow->temp1, DBRow->temp2,
             DBRow->rh, DBRow->press, DBRow->light);
    if (DEBUG) printf("MySQL insert command:\n    %s\n", sqlString);
    if (mysql_query(mysql, sqlString) != 0) { // add the row
        fprintf(stderr, "?MySQL INSERT statement failed\n\t%s\n", mysql_error(mysql));
        mysql_close(mysql);
        exit(EXIT_FAILURE);
    };
    return;
#endif
}; // end appendToDB

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return (0);
}; // end int callback()
