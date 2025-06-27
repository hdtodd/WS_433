# WS_433 Principles of Operation

WDL_433 monitors an rtl_433 server feed for weather-related, JSON packets published via MQTT and records specific fields from those JSON packets into an SQL database table.  The WWW_433 web tools select fields and records from that SQL database for graphical display on web pages using Google Chart tools.

However, others may want to select fields other than those chosen in this repository's code, or they may want to archive data from something other than remote weather sensors.  This PO manual reviews factors that impacted the design of the system and provides guidance on the operation of WDL and WWW and possible customizations.

## Factors to Consider in Design and Customization

WS_433 was designed to provide graphical presentation of historical meteorological data via web browser.  It replaces WeatherStation, a system that collects data from a single sensor (an Arduino Uno) connected to the controlling computer by USB. The WS_433 implementation collects data from multiple, remote sensors.  And that presents a number of issues in the design of the program.

###  Why JSON format?

There are many types of remote sensors, with many different combinations of data fields.  One of the strengths of rtl_433 is that it can collect and label the field values in JSON format.  That JSON format allows a client program, such as WDL_433, to extract the fields the end-user wants to monitor without concern for field positions within records or changes to the encoding program's field layout.

###  Why SQL database?

SQL databases are well supported by a variety of programming languages, and there are many tools to retrieve and process or display data from SQL databases.  Since the purpose of the WS_433 system is to display historical data, the ability to retrieve data quickly based on a timestamp made it a good choice for storage

###  Multiple messages per packet

Since it is a simplex communication system with no ability to confirm receipt of the message, most ISM-band remote sensors pack multiple copies (3-6) of a message (containing the individual temperature, humidity, barometric pressure, wind speed, etc.) into one broadcast transmission packet to improve the chance that the message will be received despite radio interference.  rtl_433 reports each of those individual messages, so there are multiple messages per sensor with the same (or nearly so) timestamps.  WS_433 needs only one reading per sensor per timestamp, so duplicated messages (same SensorID, same timestamp within 2 seconds of each other) are discarded.

###  Remote sensors send packets every 30-60 seconds

A historical view of weather does not need readings to be recorded every minute.  And recording each sensor every minute or so would cause the database to grow very quickly with nearly-redundant data.  So WDL_433 records sensor readings no more frequently than 5 minutes apart (default setting).  The recordings are not synchronized, since sensors' timings all differ, but the 5-minute threshhold results in readings that average nearly 5 minutes apart for each individual sensor (remembering that more remote sensors may not be received routinely at all!).

###  Database size and data throughput

De-duplicating records and recording no more often than every 5 minutes for each sensor both reduces the rate of growth of the database and the processing demand on the program.  As a result, WDL_433 **seems** to perform well as a single-thread program.  Increasing the frequency of recording (less than 5 minutes between records for a sensor) or recording all messages from a sensor (not de-duplicating) would likely require a more complex, threaded, queued system to keep up with the data flow.

###  No key field in databases

It might seem helpful to have the date-time column, 'date_time', be a SQL KEY field.  But since several sensors might (do!) broadcast within the same second, the key would not be unique and the INSERT command that inserts records into the database would fail.  Retrieval performance is still quite acceptable without having that field be KEY.

###  Identifying sensors: Battery change causes ID change

Changing the battery in a remote sensor usually requires re-synching it with its local monitor, and it usually causes the remote sensor's id to change.  So the sensor that had been "model" Acurite 609TXC with "id" 46 in rtl_433's JSON packet might be identified as "model" Acurite 609TXC with "id" 15 after a battery change.  A concatentation of "model"/"id"/"channel" to create a "SensorID" provides a good identifier for any particular sensor, but because of the "id" value changing with a battery change, a display of long-term historical would be difficult to create across those changed SensorID's.

So WDL_433 offers the ability identify a sensor characterized by a particular "SensorID" with a familiar name.  For example, readings from SensorID "Acurite-609TXC/46/" can be labeled as having come from a sensor labeled "Deck" in the SQL database, reflecting the fact that I know that that particular sensor is on our deck.  The section `[aliases]` in`WDL_433.ini` associates your familiar name with the ID generated by `rtl_433`.

The system manager will need to edit that file and restart WDL_433 when the battery of a known and identifiable sensor has been changed, but other WS_433 components will continue to function without changes.

###  How to determine if a packet is weather related

Remote sensors don't broadcast messages that have a field that say "I'm a weather sensor!", so it takes some guessing to decide whether or not to record in the SQL database any particular JSON record.  One clue might be if the JSON packet has a field labeled "temperature", but that is not definitive.

WDL_433 is liberal in its interpretation of what might be a weather sensor packet.  If the packet has a field that says "TPMS" (tire pressure monitoring system), it is discarded.  Otherwise, if it has a field labeled "temperature_C", it is recorded.

As a result, readings from a number of other types of sensors, notably soil sensors, refrigerator/freezer sensors, etc., are also recorded in the SQL database.  Suggestions for other filters to remove such extraneous sensor readings would be welcome.

This issue of filtering extraneous sensor packets might be particularly important if you want to customize WDL_433 to record sensor readings from some other particular type of sensor.  

##  Program Structure

WDL_433 is compiled from a number of different files.  Here is the basic structure:

|File            | Role |
|:---------------|:-----|
|WDL_433.c       | Instantiates global variables and assigns initial values to parameters such as database names, etc.; controls overall program flow|
|WDL_433.h       | Contains `typedefs` and `struct` definitions used by various other program modules; contains the definitions of default values for most of the parameters |
|WDL_cmds.c      | Contains the tables of command-line options (`getopt_long()` format) and configuration file options (indexed to command-line options) |
|GetSetParams.c, .h  | Processes configuration (.ini) file parameter settings and command-line parameters to set parameter values in global variables |
|WDL_procs.c     | Contains general utility procedures and "setters" for global variable parameters that can be changed by configuration file or command-line options |
|WDL_DBMgr.c     | Initializes SQL database (both sqlite3 and MySQL are handled here); creates database and table if necessary; appends data records to database |
|mjson.c, .h     | Deserializes JSON packets |
|Makefile        | Compiles and/or installs WDL_433 and components |

In general, the functionality is well segmented among those modules: modules other than WDL_DBMgr.c don't "know" anything about the database operations, for example.

The primary exception, of course, is with regard to the fields selected from the JSON packet and recorded in the SQL database.  The selection of fields to extract from the JSON packet is managed in WDL_433.c, and the recording of data values into columns in the SQL database is handled in WDL_DBMgr.c.  The two are linked by a `struct` variable that contains the data to be recorded, which is passed from WDL_433.c to WDL_DBMgr.c.  Suggestions on how to customize those elements are provided below.

## WDL_433 Basic Operation

### Procedure Flow

The main procedure, in WDL_433.c:

*  compiles into global variables the default values of the operational parameters from definitions in the .h file;
*  in execution, first sets the interrupt-handler to catch \<Control-C\>'s to terminate execution cleanly;
*  invokes GetSetParams() to process the configuration (.ini) file (first) and then the command-line options to change any parameters they might set: precedence for overrides is command-line \> configuration file \> default;
   * GetSetParams() validates the command tables to ensure that config file and command line processing will match;
   * reads and sets or responds to any special command-line options (`-d`, `-g`, `-h`, `-v`);
   * reads and sets any parameters identified in the configuration file (default: `WDL_433.ini`, but can be changed by the `-c <path>/<file>` command-line option);
   * re-reads and sets any command-line options and processes any sensorID-alias name associations into a binary search tree for lookups as messages are received;
   * then returns control to the `main()` procedure in `WDL_433.c`;
*  connects to the rtl_433 server MQTT service to confirm that the service is active;
*  invokes a WDL_DBMgr procedure to check that the database can be accessed and creates the database and table if necessary;
*  subscribes to the MQTT stream and provides a callback procedure that the MQTT library invokes when an MQTT packet is received
*  enters a run loop that continues until the program is terminated by \<Control-C\>.

The MQTT callback procedure:

* deserializes the JSON message into fields in a `struct DBRecord`,
* determines if the message is a type that it should record,
* if it is, checks the date-time stamp of the prior message from that sensor to see if it is a "new" sensor reading,
* if it should be recorded, invokes a procedure in WDL_DBMgr.c to append the data in the `struct` variable to the database.

###  Debugging

WDL modules have extensive debugging `printf` statements embedded to assist with debugging, and there are two configuration settings that that can be helpful: `-g` or `--Gdebug` enables debugging in the `GetSetParams.c` module that processes the configuration file and command-line options; and `-d` or `--debug` enables debugging in the remainder of the program.  The variables GDEBUG and DEBUG that are set by these options are global variables, with values established in the main `WDL_433.c` module.  They are initially `bool` values of `false`: change them in that module if you want to enable debugging information by default.  They may also be set in the configuration file or by the command-line switch.

### WDL Databases

WDL can record the date-time stamped sensor data it receives from rtl_433 in either a sqlite3 (default) or MySQL database.  Operations are similar for each database type:

*  On startup, using sqlite3, if the database file doesn't exist, WDL_DBMgr.c creates it, and if the table doesn't exist, WDL_DBMgr.c creates it using the command:
```
CREATE TABLE IF NOT EXISTS SensorData (date_time TEXT, sensorID TEXT,
   temp1 REAL, temp2 REAL, rh REAL, press REAL, light REAL);
```
*  Similarly, on startup using MariaDB/MySQL, if the database file "Weather" doesn't exist, WDL_DBMgr.c creates it, and if the table "SensorData" doesn't exist, WDL_DBMgr.c creates it with:
```
CREATE TABLE IF NOT EXISTS `SensorData` (`date_time` char(20), `sensorID` char(50),
  `temp1` float, `temp2` float, `rh` float, `press` float, `light` float)
```

During operation, as the MQTT callback routine invokes `appendToDB(&DBRec)` to append data to the database, the `appendToDB` procedure uses a SQL query command of the form:
```
INSERT INTO SensorData (date_time, sensorID, temp1, temp2, rh, press, light)
VALUES ('2025-04-30 16:53:13', 'Deck',  20.2,   0.0,  23,    0.0,   0)
```
to append to the database.

The sqlite3 database file is opened and then immediately closed when recording each individual sampling, so that the file is minimally vulnerable to corruption in case of system crash.  The MariaDB/MySQL database is left connected from when it is initialized until the program terminates.

The databases can be examined as a normal database table, for example, with the command:

```
$sqlite3 /var/databases/Weather.db
sqlite> select * from SensorData;
2025-04-28 15:46:08|Neighbor|24.7|0.0|23.0|0.0|0.0
2025-04-28 15:46:53|Deck|22.8|0.0|12.0|0.0|0.0
2025-04-28 15:47:25|Office|25.0|24.0|46.0|1021.9|73.0
2025-04-28 15:49:23|LaCrosse-TX141THBv2/221/0|23.0|0.0|17.0|0.0|0.0
```

If you delete the sqlite3 database file, it will be recreated when WDL_433 is next invoked.  Similarly, if you delete the table or database on the MySQL server, it/they will be recreated when WDL_433 is next invoked.

## Customization

If you want to select a different set of JSON fields to record in a SQL database, or add to the set of fields already being recorded, start by identifying the labels and value types of the fields in the rtl_433 JSON messages for your sensor type.

Then, the key elements to edit will be:

*  The `struct` called "DBRecord" that is defined in WDL_433.h;
   *  Retain the "date_time" and "sensorID" fields
   *  Remove any fields you don't intend to record
   *  Add, with field name and value type any fields you want to add
*  In WDL_433.c, find the structure `const struct json_attr_t json_rtl[]`.  That is the structure that tells mjson.c which fields you want to extract from the JSON packet, the type of data, and where to store the deserialized values.  Leave "date_time" and "sensorID" as they are; remove any fields you don't want to record, and add fields you do want to record, using the field name in "DBRow" as the storage location.
*  In `message_callback()` in WDL_433.c, do any ancillary processing you might want to do on the DBRow fields you've introduced (convert Centigrade to Fahrenheit, etc.).  In the DEBUG printf statements, replace the existing field names with the ones you've introduced.
*  In WDL_DBMgr.c, modify the "CREATE TABLE" commands to use your field names and types (again, leave "date_time" and "sensorID" as they are).
*  In WDL_DBMgr.c, modify the "INSERT INTO" commands to use your field values (from structure `DBRow`) in the same order you listed in the "CREATE TABLE" command.
*  To facilitate testing without replacing prior tables, edit WDL_433.h to set different default database name, table name, etc.

Compile using `make` and resolve any compilation errors resulting from your edits.

Test with `./WDL_433 -g -d` so that you can trace execution of the code in case run-time errors occur.

## WWW_433

If you customize WDL_433 to record different fields in the SQL database, the PHP and Python files in WWW_433 won't work ... or, rather, will likely display a graph but have all documentation incorrect.  You'll need to read through the code to see that the fields are selected by _column number_ rather than name.  So identify the columns of the data you want to display, select those columns from the SQL database, and change the labels for the axes and header.

But if you got the customization of WDL_433 to work corrrectly, fixing the graphs will be a piece of cake.

## Author

David Todd, hdtodd@gmail.com, 2025.04.30

If you customize, I'd appreciate feedback on your experience and how this document might be improved to make it easier.


