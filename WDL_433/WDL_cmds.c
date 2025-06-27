/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*
    WDL_cmds.c

    Table of commands that WDL_433 will recognize, either through .ini file
    or through command-line options.

    Start with the switched that indicate what operation can set the parameter
    or invoke the option, whether that option has already been set, and whether
    the option is required for program operation.
    
    Switches for setting operational parameters
    SWRQD ==> the parameter MUST be set for operation (prompt if necessary)
    SWINI ==> the .ini file can set the parameter
    SWCLI ==> a command-line option can set the parameter
    SWSET ==> the parameter HAS been set (assert for params with defaults)

    x ==> set to "true" in cmdlist.long_opt
    c ==> set to "true" conditionally (depends on sqlite3/MySql setting)

    Parameter   reqd     ini     cli    set
    debug                 x       x
    Gdebug                x       x
    help                          x
    config                        x
    source                x       x     x    //disabled for now
    host         x        x       x
    port         x        x       x     x
    topic        c        x       x     x
    sql3path     c        x       x     x
    sql3file     c        x       x     x
    myhost       c        x       x
    myuser       c        x       x
    mypass       c        x       x
*/

#include "WDL_433.h"

/*
    auxdata supplements the information in getopt_long()'s
    getopt list with information needed for .ini and command-line
    setting, a pointer to the procedure that can set the value for
    the selected option, and a description of the option that
    can be used for the --help option response.
*/

/* clang-format off */
static auxdata optdetails[] = {
    //ltr switches                 &setter function      desc  
    {'c', SWCLI,                   NULL,                 "Path/name for configuration file"},
//  {'S', SWINI|SWCLI|SWSET,       (void *)&setSource,   "Source protocol [ MQTT | HTTP ]"},
    {'H', SWRQD|SWINI|SWCLI,       (void *)&setHost,     "Name or IP of MQTT or HTTP host"},
    {'P', SWRQD|SWINI|SWCLI|SWSET, (void *)&setPort,     "Port number of MQTT or HTTP host"},
    {'T', SWRQD|SWINI|SWCLI,       (void *)&setTopic,    "MQTT publisher topic to monitor"},
#ifdef USE_SQLITE3
    {'q', SWRQD|SWINI|SWCLI|SWSET, (void *)&setSql3path, "Path to sqlite3 database file"},
    {'s', SWRQD|SWINI|SWCLI|SWSET, (void *)&setSql3file, "Name of sqlite3 database file"},
#else
    {'m', SWRQD|SWINI|SWCLI,       (void *)&setMyHost,   "MySQL host Name or IP"},
    {'u', SWRQD|SWINI|SWCLI,       (void *)&setMyUser,   "MySQL username"},
    {'p', SWRQD|SWINI|SWCLI,       (void *)&setMyPass,   "MySQL password"},
#endif
    {'D', SWINI|SWCLI,             (void *)&setDebug,    "Print WDL debugging information"},
    {'G', SWINI|SWCLI,             (void *)&setGDebug,   "Print GetSetParams() debugging information"},
    {'h', SWCLI,                   (void *)&helper,      "This help message"},
    {'v', SWCLI,                   NULL,                 "Print program version number"},
    {0,   0,                       NULL,                  NULL}
};
/* clang-format on */

//  'short_opt' is the standard getopt() list of single-letter options
//  'optaux'    is the auxiliary list above to help with .ini and CLI processing
//  'long_opt'  is the standard getopt_long() list of verbose CLI options
//  The indices for the 'long_opt' 'ltr' value and the 'optaux' 'val' value
//  must be the same (validated by GetSetParams())
static cmdlist_t cmdlist = {
#ifdef USE_SQLITE3
    .short_opt = "c:H:P:T:q:s:DGhv",
#else
    .short_opt = "c:H:P:T:m:u:p:DGhv",
#endif
    .optaux = optdetails,
    .long_opt = {
	//name       has_arg            flag  ltr
	{"config",   required_argument, NULL, 'c'},
//  {"source",   required_argument, NULL, 'S'},
	{"host",     required_argument, NULL, 'H'},
	{"port",     required_argument, NULL, 'P'},
	{"topic",    required_argument, NULL, 'T'},
#ifdef USE_SQLITE3
    {"sql3path", required_argument, NULL, 'q'},
    {"sql3file", required_argument, NULL, 's'},
#else
    {"myhost",   required_argument, NULL, 'm'},
    {"myuser",   required_argument, NULL, 'u'},
    {"mypass" ,  required_argument, NULL, 'p'},
#endif
    {"debug",    no_argument,       NULL, 'D'},
    {"Gdebug",   no_argument,       NULL, 'G'},
    {"help",     no_argument,       NULL, 'h'},
    {"version",  no_argument,       NULL, 'v'},
    {NULL,       0,                 NULL,  0 }
    }
};
