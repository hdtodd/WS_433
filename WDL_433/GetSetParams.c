/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*
    GetSetParams.c
    Get program operational parameters from a .ini file and
    then from command line and set them in program variables

    This procedure uses a 'long_opt' list for the 'getopt_long()'
    procedure, integrated with an auxiliary table, 'optaux', to
    provide a list of parameters that can be set from a .ini
    configuration file or from the command line.

    The procedure does a prelimiary scan of the command line
    to determine if there is an explicitly-provided .ini file
    or if there are commands that can be handled immediately,
    such as --help or --version.

    The precedence for setting parameters then is as follows:
      - The invoking program may have set default values for the
        various parameters
      - The .ini file may change some of those parameters
      - The command line options may change some of those parameters
      - If critical parameters have not been set at the end of the
        procedure, the user is prompted for parameters through the
        terminal interface

    The .ini file may be specified with a command-line '-c' option;
    if it isn't, this procedure searches the path list to see if the
    default file exists somewhere in that path.  If it finds a file,
    use it before re-processing the command line.

    The 'optaux' table indicates which stage (.ini, cli) may set
    each parameter and which parameters are required to be set for
    the main program to proceed.
    
    Adapted and integrated from various human and AI sources
    HDTodd@gmail.com, 2025.04.23
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include "GetSetParams.h"
#include "WDL_433.h"

// GDEBUG is for debugging this procedure
// DEBUG is for general debugging outside of this procedure
extern bool GDEBUG;
extern bool DEBUG;

/*
    Start by validating that the indices between 'long_opt' and
    the 'optaux' tables are referring to the same commands.

    Then get program operational parameters, first from a '.ini'
    config file if there is one, and then from the command line
*/
void GetSetParams(int argc, char *argv[], cmdlist_t *cmdlist) {
    int cmd_index;
    bool dfltIni = true;
    int c;
    FILE *cFile;
    char iniFile[FNLEN+1];
    char dfltname[] = INI_FILE;
    char paths[] = INI_PATH;
    char *path;
    
    // Procedure to find the index of short_opt char 'c' in the 
    // long_opt table.  Return -1 if not found
    int findLongIndex(char c, struct option *long_opts) {
      for (int i=0; long_opts[i].name != NULL; i++) {
        if (c==long_opts[i].val) return i;
      };
      return -1;
    };

    if (GDEBUG) printf("'--Gdebug' enabled\n");

    // First, confirm that the .long_opt[] and .optaux[]
    // tables are aligned correctly by checking the one-letter options
    for (int i=0; cmdlist->long_opt[i].name!=NULL; i++) {
        if (cmdlist->long_opt[i].val != cmdlist->optaux[i].ltr) {
    	    fprintf(stderr, "The command tables .long_opt and .optaux are misaligned\n");
	        fprintf(stderr, "Check row %d in those tables, for command --%s, letter -%c\n",
		            i+1, cmdlist->long_opt[i].name, cmdlist->long_opt[i].val);
            exit(1);
        };
    };
    
    // Next, check for settings we may/must handle before others
    // '--debug', '--config', '--help',  and '--version' are handled here
    if (GDEBUG) printf("Initial settings check for '-d', '-G', '-c', '-h', and '-v'\n");
    optind = 0;
    while (1) {
        cmd_index = -1;
        c=getopt_long(argc, argv, cmdlist->short_opt,
                      (const struct option *)cmdlist->long_opt, &cmd_index);
        if (c==-1) break;
        switch (c) {

            // Help?
            case 'h':
                helper(cmdlist);
                exit(0);
                break;
                
            // set DEBUG?
            case 'd':
                DEBUG = true;
                printf("'--debug' enabled from command line\n");
                break;

            // set GDEBUG?
            case 'G':
                GDEBUG = true;
                printf("'--Gdebug' enabled from command line\n");
                break;

            // set a configuration file?
            case 'c':
                dfltIni = false;
                strcpy(iniFile, optarg);
                break;

            case 'v':
                printf("%s %s\n", APP_NAME, APP_VERSION);
                exit(0);

             // ignore everything else for now
            default:
        };
    };
    if (GDEBUG) printf("Finished initial settings loop\n");

    // Next, open the .ini (configuration) file if there is one
    if (GDEBUG) printf("\n\nProcess .ini file if there was one\n");
    // Did the command line provide the name of a .ini file?
    if (!dfltIni) {
        // Yes. Open the .ini file specified with '-c'
        cFile = fopen(iniFile, "r");
        if (cFile == NULL)
            fprintf(stderr, "?Configuration file '%s' not found\n", iniFile);
        else
            if (GDEBUG) printf("Using config file %s\n", iniFile);
    } else {
        // No '-c' specified; search the INI_FILE paths
        char *fullpath = (char *)malloc(2*FNLEN * sizeof(char));
        if (GDEBUG)
            printf("Searching these paths for the .ini file: %s\n", INI_PATH);
        path = strtok(paths, ":");
        while (path != NULL) {
            sprintf(fullpath, "%s/%s", path, dfltname);
            if (GDEBUG) printf("Try fullpath: %s\n", fullpath);
            cFile = fopen(fullpath, "r");
            if (cFile != NULL) {
                if (GDEBUG) printf("Using config file %s\n", fullpath);
                break;
            };
            path = strtok(NULL, ":");
        };
        free(fullpath);
    };

    // If we opened a .ini file, process it
    if (GDEBUG) printf("Begin processing configuration ('.ini') file\n");
    if (cFile != NULL) {
        IniData data;
        initIniData(&data);
        if (parseIniFile(cFile, &data)) {
            if (GDEBUG) printf("Parsed .ini data:\n");
            for (int i = 0; i < data.count; i++) {
                if (GDEBUG) printf("\tSection: %s, Key: %s, Value: %s\n",
                                  data.entries[i].section,
                                  data.entries[i].key, data.entries[i].value);
            };
        }
        else {
            fprintf(stderr, "Could not parse .ini file '%s'.\n", cFile);
        };

        // Finally, we have the key:value dictionary of command-line
        // parameters, so process the ones we're allowed to
        for (int i = 0; i < data.count; i++) {
            int cmd;
            bool foundCmd = false;
            strLower(data.entries[i].key);
            // Is this key in the list of commands?
            for (cmd=0; cmdlist->long_opt[cmd].name!=NULL; cmd++)
                if (strcmp(cmdlist->long_opt[cmd].name, data.entries[i].key) == 0) {
                    // got a match
                    foundCmd = true;
                    break;
                };
            if (foundCmd) {
                // Found the key in the command table.  Change its value if we can.
                if ( (cmdlist->optaux[cmd].switches & SWINI) != 0 ) {
                    // OK, we can change this key.  Ask setter to do it
                    cmdlist->optaux[cmd].setter(data.entries[i].value);
                    cmdlist->optaux[cmd].switches |= SWSET;
                    if (GDEBUG)
                        printf("\tFrom .ini, setting %s to %s\n",
                               cmdlist->long_opt[cmd].name, data.entries[i].value);
                } else {
                   fprintf(stderr, "\nSetting '%s' from .ini file not permitted\n",
                           data.entries[i].key);
                }
            } else {
                //  Key not found in command table; say so
                fprintf(stderr, "Unrecognized .ini keyword '%s'\n", data.entries[i].key);
            };
        };
        freeIniData(&data);
        fclose(cFile);
    };  //finished processing .ini file
    if (GDEBUG) printf("Finished processing the .ini file\n");

    // Now process command line switches
    if (GDEBUG) printf("\n\nProcess the command line as a rescan\n");
    optind = 0;
    while (1) {
        cmd_index = -1;
        c=getopt_long(argc, argv, cmdlist->short_opt,
                      (const struct option *)cmdlist->long_opt, &cmd_index);
        if (cmd_index == -1) cmd_index = findLongIndex(c, cmdlist->long_opt);
        if ( (c==-1) || (cmd_index == -1) ) break;
        if ( (c=='c') || (c=='d') || (c=='G') || (c=='v')) continue;
        // Haven't processed this command-line option yet, so process it
        // Can this be changed from the command line?
        if ( (cmdlist->optaux[cmd_index].switches & SWCLI) != 0 ) {
            //  Yes.  Do it.
            if (GDEBUG) printf("Setting --%s to %s from command line\n",
                              cmdlist->long_opt[cmd_index].name,
                              (optarg == NULL) ? "" : optarg);
            cmdlist->optaux[cmd_index].setter(optarg);
            cmdlist->optaux[cmd_index].switches |= SWSET;  // note that it's set
        } else {
           fprintf(stderr, "\nSetting '%s' from command line not permitted\n",
                   cmdlist->long_opt[cmd_index].name);
        };
    };
    if (GDEBUG) printf("Finished processing command line\n");

    // And finally, make sure all required parameters have been set
    // Prompt for any that haven't been set
    if (GDEBUG) printf("\n\nPrompt for any required params that have not been set\n");
    for (int cmd=0; cmdlist->long_opt[cmd].name != NULL; cmd++) {
        char response[FNLEN+1];
        // Is this parameter required but not yet set?
        if ( ( (cmdlist->optaux[cmd].switches & SWRQD)!=0)
             && (cmdlist->optaux[cmd].switches & SWSET)==0 ) {
            // Yes.  Prompt for it and set it.
            printf("%s: ", cmdlist->optaux[cmd].desc);
            scanf("%s",response);
            if (GDEBUG) printf("Setting %s to %s\n",
                              cmdlist->long_opt[cmd].name, response);
            cmdlist->optaux[cmd].setter(response);
            cmdlist->optaux[cmd].switches |= SWSET;
        };
    };
};  // end GetSetParams()


/*
    The key:value data from the .ini are stored into a table for
    processing into the invoking program's parameters
    (by setter procedures) after the .ini file has been digested.
 */
void initIniData(IniData *data)
{
    data->entries  = NULL;
    data->count    = 0;
    data->capacity = 0;
}

void freeIniData(IniData *data)
{
    free(data->entries);
    initIniData(data);
}

int addEntry(IniData *data, const char *section,
             const char *key, const char *value) {
    if (data->count >= data->capacity) {
        data->capacity    = (data->capacity == 0) ? 4 : data->capacity * 2;
        Entry *newEntries = realloc(data->entries, data->capacity * sizeof(Entry));
        if (newEntries == NULL) {
            return 0;
        }
        data->entries = newEntries;
    };

    strncpy(data->entries[data->count].section, section, MAX_SECTION_LENGTH - 1);
    data->entries[data->count].section[MAX_SECTION_LENGTH - 1] = '\0';
    strncpy(data->entries[data->count].key, key, MAX_KEY_LENGTH - 1);
    data->entries[data->count].key[MAX_KEY_LENGTH - 1] = '\0';
    strncpy(data->entries[data->count].value, value, MAX_VALUE_LENGTH - 1);
    data->entries[data->count].value[MAX_VALUE_LENGTH - 1] = '\0';
    data->count++;
    return 1;
};

int parseIniFile(FILE *file, IniData *data) {
    char line[MAX_LINE_LENGTH];
    char currentSection[MAX_SECTION_LENGTH] = "";

    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
        char *comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        };

        int len = strlen(line);
        while (len > 0 && isspace(line[len - 1])) {
            line[len - 1] = '\0';
            len--;
        };

        if (line[0] == '\0')
            continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end != NULL) {
                *end = '\0';
                strncpy(currentSection, line + 1, MAX_SECTION_LENGTH - 1);
                currentSection[MAX_SECTION_LENGTH - 1] = '\0';
            }
        }
        else {
            char *equals = strchr(line, '=');
            if (equals != NULL) {
                *equals     = '\0';
                char *key   = line;
                char *value = equals + 1;

                while (isspace(*key))
                    key++;
                len = strlen(key);
                while (len > 0 && isspace(key[len - 1])) {
                    key[len - 1] = '\0';
                    len--;
                }
                while (isspace(*value))
                    value++;
                len = strlen(value);
                while (len > 0 && isspace(value[len - 1])) {
                    value[len - 1] = '\0';
                    len--;
                }

                if (!addEntry(data, currentSection, key, value)) {
                    fclose(file);
                    freeIniData(data);
                    return 0;
                }
            }
        }
    }
    return 1;
};

char *getValue(IniData *data, const char *section, const char *key) {
    for (int i = 0; i < data->count; i++) {
        if (strcmp(data->entries[i].section, section) == 0 && strcmp(data->entries[i].key, key) == 0) {
            return data->entries[i].value;
        }
    }
    return NULL;
};
