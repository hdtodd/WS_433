/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
/*
    WDL_433
    Weather data logger for rtl_433
    v1.0

    This program logs weather data published as JSON packets by a rtl_433
    server into a sqlite3 or MariaDB/MySQL database (compile-time option).

    hdtodd@gmail.com
    2025.04.14
    2025.06.27  Updated to incorporate aliases into WDL_433.ini
*/

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <mosquitto.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "WDL_433.h"
#include "GetSetParams.h"
#include "mjson.h"

void GetSetParams(int argc, char *argv[], cmdlist_t *cmdlist);

// Make operating parameters global so all modules can see them
bool     DEBUG    = false; 
bool     GDEBUG   = false; 
source_t source   = MQTT;
char    *host     = "";
int      port     = 1883;
char    *topic    = "";
NPTR    sensors   = NULL;

#ifdef USE_SQLITE3
bool   usingSql3 = true;
char   *sql3path = DBPATH;
char   *sql3file = DBNAME".db";
#else
bool   usingSql3  = false;;
char   *myHost    = "";
char   *myUser    = "";
char   *myPass    = "";
#endif

struct mosquitto *mosq;
bool run = true;
char lastSensorID[201] = "";
time_t timestamp;
char id[20];
char chnl[20];
time_t lasttime = (time_t) 0x00000000;
struct tm tm;
char paths[] = INI_PATH;
char *path;
DBRecord  DBRow;

// This is the list of JSON fields that will be processed into the database
const struct json_attr_t json_rtl[] = {
  {"time",            t_string, .addr.string = DBRow.date_time, .len = sizeof(DBRow.date_time)},
  {"model",           t_string, .addr.string = DBRow.sensorID,  .len = sizeof(DBRow.sensorID)},
  {"id",              t_string, .addr.string = id,              .len = sizeof(id)},
  {"channel",         t_string, .addr.string = chnl,            .len = sizeof(chnl)},
  {"temperature_C",   t_real,   .addr.real   = &DBRow.temp1},
  {"temperature_2_C", t_real,   .addr.real   = &DBRow.temp2},
  {"humidity",        t_real,   .addr.real   = &DBRow.rh   },
  {"pressure_hPa",    t_real,   .addr.real   = &DBRow.press},
  {"light_pct",       t_real,   .addr.real   = &DBRow.light},
  {"",                t_ignore},
  {NULL}
};

void handle_signal(int s) {
    run = false;
    mosquitto_disconnect(mosq);
}

// This is called when the MQTT connection to the server has been made or
//   re-established.  It subscribes or re-subscribes to the topic so that
//   messages will be received and processed by the message callback routine..
void connect_callback(struct mosquitto *mosq, void *obj, int connack_code) {
    if (DEBUG) {
        printf("MQTT connect callback, result code = %d\n", connack_code);
        printf("MQTT result msg: %s\n", mosquitto_connack_string(connack_code));
    };
    if (connack_code >= 0x80) {
        fprintf(stderr,"MQTT connection failed with error code %d!\n", connack_code);
        exit(EXIT_FAILURE);
    }
    int rc = mosquitto_subscribe(mosq, NULL, topic, 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "?Couldn't subscribe to MQTT server '%s', port %d, topic '%s'\n",
                host, port, topic);
        fprintf(stderr, "Subscription error code %d, \n   %s\n",
                rc, mosquitto_reason_string(rc));
        fprintf(stderr, "Verify that the topic and port are correct\n");
        exit(EXIT_FAILURE);
    };
};

// This processes the JSON packets as received by the MQTT client procedure
// Ignores tire pressure messages and messages that don't have temperature
// readings.  Only records messages from any individual sensor approximately
// every 5 minutes.
void message_callback(struct mosquitto *mosq, void *obj,
                      const struct mosquitto_message *message) {
    int jstatus;

    // [NOTPMS] Ignore tire pressure readings
    if (strstr(message->payload, "TPMS") != NULL) return;
    // [REQUIRETEMPERATURES] Ignore if message doesn't have a temperature reading
    if (strstr(message->payload, "temperature") == NULL) return;
    // Got a message: deserialize it
      jstatus = json_read_object(message->payload, json_rtl, NULL);
      // If not successful, say so and give up on this record
      if (jstatus != 0) {
          fprintf(stderr,json_error_string(jstatus));
          return;
      };
      // Convert the time string to a 'time_t' entity for comparisons
      strptime(DBRow.date_time, "%Y-%m-%d %H:%M:%S", &tm);
      tm.tm_isdst = 1;
      timestamp = mktime(&tm);

      // The statements below appends the 'id' and 'chnl' fields to
      // 'sensorID' to make 'model'/'id'/'chnl' the key to use
      // for identifying the sensor and for de-duplicating records
      strcat(DBRow.sensorID, "/"); strcat(DBRow.sensorID, id);
      strcat(DBRow.sensorID, "/"); strcat(DBRow.sensorID, chnl);

      // Now de-dup the packets as received: Ignore duplicated readings in the same message
      // If sensorID has not changed or time < 2 sec since last record, ignore it
      if ( (strcmp(DBRow.sensorID, lastSensorID) == 0) &&
           (timestamp < lasttime+DUP_REC) ) return;

      // OK, need to record the data for this sensor.
      // First, see if we've seen it since startup so we can record this timestamp
      //   and if we haven't seen it before, create a new node
      NPTR node = node_find(sensors,DBRow.sensorID,true);
      if (sensors == NULL) sensors = node;
      if (node == NULL) {
        fprintf(stderr, "Couldn't record for sensorID %s\n", DBRow.sensorID);
        return;
      };
      
      // Record its sensorID and timestamp so we can wait for
      // 'DUP_REC' seconds before making another entry
      strcpy(lastSensorID, DBRow.sensorID);
      lasttime = timestamp;

      // If we've seen this sensorID less than 'recordingInterva' seconds
      // in the past, don't record it now
      if (timestamp < node->lasttime+recordingInterval) return;
      
      // If there is a known alias for this sensor, use it in the database entry
      if (node->alias != NULL) strcpy(DBRow.sensorID, node->alias);

      // Append this entry to the database and note the recording 
      appendToDB(&DBRow);
      node->lasttime = timestamp;
      
      if (DEBUG) 
          printf("%s: %20s, %5.1f\u00B0C\t%5.1f\u00B0C\t%4.0f%% RH\t%6.1f h_Pa\t%4.0f%% light\n",
                 DBRow.date_time, DBRow.sensorID,
                 DBRow.temp1, DBRow.temp2,
                 DBRow.rh, DBRow.press, DBRow.light);
      return;
};

/*
    The main procedure first sets the operational parameters, by default values,
    .ini (configuration) file, or command-line processing; checks that the
    database can be accessed; then subscribes to the designated publication stream
    from the rtl_433 server and lets those packets be processed by the
    MQTT callback procedure until the program is terminated.
*/
int main(int argc, char *argv[]) {
// Instantiate the command list and options auxilliary tables
#include "WDL_cmds.c"
    uint8_t reconnect = true;
    char clientid[24];
    enum mosq_err_t rc;
    const int KEEPALIVE = 60;
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("WDL_433: Weather station data logger for rtl_433 servers\n");

    if (DEBUG) PrintParams(&cmdlist,
        "Initial values for operating parameters before .ini and CLI processing\n");

    GetSetParams(argc, argv, &cmdlist);
    
    if (DEBUG) {
        PrintParams(&cmdlist,
            "Final values for operating parameters after .ini and CLI processing");
        printf("Sensor aliases from .ini file: \n");
        tree_print(sensors);
    };

  // Create the MQTT client
    if (DEBUG) printf("Opening MQTT connection & subscribing\n",
                      "Host: %s, port %d, topic: %s\n",
                      host, port, topic);
    mosquitto_lib_init();
    snprintf(clientid, sizeof(clientid), "WDL_433_%d", getpid());
    mosq = mosquitto_new(clientid, true, 0);
    if (mosq == NULL) {
        fprintf(stderr, "?WDL_433: Unable to create mosquitto client\n");
        exit(EXIT_FAILURE);
    };
    
    // Check for database file or open MySQL connection
    initDBMgr();

    //  Connect to the MQTT feed and subscribe in connect_callback
    if (DEBUG) printf("Subscribing to MQTT feed\n");
    mosquitto_connect_callback_set(mosq, connect_callback);
    mosquitto_message_callback_set(mosq, message_callback);
    rc = mosquitto_connect(mosq, host, port, KEEPALIVE);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "?WDL_433: Couldn't connect to MQTT server\n");
        fprintf(stderr, "Verify that host '%s' is publishing MQTT on port %d,\n",
                host, port);
        exit(EXIT_FAILURE);
    };

    // Main loop: run until signaled not to by CNTL-C
    // Enter wait loop with 30-sec timeout for disconnects
    // loop_forever() handles reconnects if server disconnects
    if (DEBUG) printf("Entering MQTT run loop\n");
    rc = mosquitto_loop_forever(mosq, 30000, 1);

    // Exit here when told to stop; clean up
    mosquitto_destroy(mosq);
    if (DEBUG) {
        printf("Sensors recorded in this session:\n");
        tree_print(sensors);
    };
    mosquitto_lib_cleanup();
};

