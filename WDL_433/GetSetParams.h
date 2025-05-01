/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*
    GetSetParams.h
    Definitions needed for procedures to get program operating parameters from
    .ini file and command line and set them in program variables

    HDTodd@gmail.com, 2025.04.23
*/

#pragma once

#include <getopt.h>

// GetSetParams definitions
// Switch settings for each parameter
typedef enum {SWRQ=0,SWIN,SWCL,SWSS} switch_t;
#define  SWRQD 1<<SWRQ   // param may be required (c) depending on other params
#define  SWINI 1<<SWIN   // .ini file can set
#define  SWCLI 1<<SWCL   // command line can set
#define  SWSET 1<<SWSS   // value has not been set

// GetIni parameters
#define MAX_LINE_LENGTH    256
#define MAX_SECTION_LENGTH  64
#define MAX_KEY_LENGTH      64
#define MAX_VALUE_LENGTH   128

/*
    GetSetParams structures
    "auxdata" contains auxillary data used
    to process command options.  The array
    elements must pair with the option
    "long_opt" array elements (verified
    upon startup)
*/
typedef struct {
    int  ltr;
    int  switches;
    void (*setter)(char *);
    char *desc;
} auxdata;

/*
   cmdlist_t includes the 'getopt_long()' variables
   'short_opt' and 'long_opt' and a pointer to
   the auxiliary table that describes the commands
   and points to procedures that set parameter values.
*/
typedef struct {
    char   *short_opt;
    auxdata *optaux;     
    struct option long_opt[];
} cmdlist_t;

// GetIni structures
typedef struct {
    char section[MAX_SECTION_LENGTH];
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} Entry;

typedef struct {
    Entry *entries;
    int    count;
    int    capacity;
} IniData;

// GetIni procedures
void  initIniData(IniData *data);
void  freeIniData(IniData *data);
int   parseIniFile(FILE *file, IniData *data);
char *getValue(IniData *data, const char *section, const char *key);
