# WDL_433 Installation Guide

This guide provides details for the installation of WDL_433.  It assumes that you already have a computer system running [rtl_433](http://github.com/merbanan/rtl_433) to collect ISM-band (433MHz in the US) remote sensor data, and that you have a system on which you can install WDL_433 and the necessary support tools (could be the same computer).  Installation is not particularly complicated but does require that some libraries and tools be installed already and that network and directory permissions be set.  The following section summarizes the installation of those tools and how to confirm that they're working.

## Ensuring that you have the prerequisites

This guide assumes that you have already cloned the repository onto a system that has the tools needed for compilation and operation.  The commands below are for Raspberry Pi OS; they should work for any other Debian system, but you'll need to adapt them for other operating systems.

*  mosquitto (`sudo apt-get install mosquitto-clients libmosquitto1`)
*  sqlite3-dev (`sudo apt-get install sqlite3 libsqlite3-dev`) and/or libmysqld-dev (`sudo apt-get install mariadb-client libmariadb-dev`)

You will also need an rtl_433 service running on your network to collect the ISM-band broadcasts: configure it to publish in JSON format via MQTT the packets it receives.  For example, in your rtl_433.conf file, you might have the line (under "output"): `output mqtt`.

Confirm that you can subscribe to the rtl_433 MQTT service from your WDL_433 computer by, for example, issuing the command `mosquitto_sub -h <your rtl_433 server> -t "rtl_433/<your rtl_433 server>/events"`.  You should see the received packet data in JSON format on the terminal window.

If you're using sqlite3 as your database service, confirm that your program will have write access to the directory where the sqlite3 database will be stored.  By default, the database will be stored as `/var/databases/Weather.db`.  You can change that in the `WDL_433.h` file, but ensure that your program will have access to write to whereever you want that database file to be located.

If you're using MariaDB/MySQL as your database service, confirm that you have access to that server from your WDL_433 computer and that you have the correct username and password to create and write to database files there.  For example, check with `mysql -h <MySQL host> -u <MySQL username> -p <carriage return>`; supply the MySQL password; confirm that you have access to databases with, for example, the command `show databases;`.

With that foundation confirmed, you're now ready to configure, compile, and install WDL_433.

## Quickstart

After all of the software components have been installed and you've edited any compilation configuration changes you want to make:

1.  `cd` to the directory `WS_433/WDL_433`.
2.  Type `make` to create the sqlite3 version of WDL_433 or type `make USE_MYSQL=1` to create the MariaDB/MySQL version.
3.  Type `./WDL_433 -d` to start WDL_433 in debugging mode: it will:
    *  trace its process of setting parameters,
    *  display the final parameter values,
    *  show its subscription to the MQTT service,
    *  show that it has connected to the SQL service,
    *  begin displaying the records published by the rtl_433 service as it archives them to the database.

That's it!  If you want to run WDL_433 for a while without the debugging information, stop its execution by pressing \<Control-C\> and then type `./WDL_433` to restart the program without the debugging information.  The program will simply run and append records to the database until you stop it again.

### Resolving Problems

If records are not being saved to the SQL database, WDL_433 will give an indication of what has failed:
*  no records received from the rtl_433 server: double-check access to the server from the WDL_433 computer using the `mosquitto_sub` command above;
*  unable to save to database: check database directory permissions (sqlite3) or login credentials (MariaDB) using the commands above; correct as needed.  From terminal command line, execute the sqlite3 or MySQL commands to manually create the database and table and correct any access permissions until you're successful.

If you've had trouble getting WDL_433 to operate with the instructions above or just want to change some of the parameters, the more detailed instructions below may be helpful.

### Moving into Production Mode

Once it appears that WDL_433 is functioning correctly, take these steps to verify operation and move it into production:

1.  When you've confirmed that WDL_433 is performing as expected, restart it and let it run for a few hours to collect data.
2.  Confirm that values have been stored correctly into the database.
    *  For squite3, type `sqlite3 /var/database/Weather.db` (substitute the path/filename for the database name if you changed it in `WDL_433.ini`;
    *  For MariaDB/MySQL, type `mysql -h <database host> -u <your username> -p`, supply the password, and in response to the MySQL prompt, type `use Weather`;
    *  Type `SELECT * from SensorData` to see the records that have been saved.
    *  You should see a list of records from the various sensors from which your rtl_433 server is receiving broadcasts.  
4.  Again, inspect the list of records and confirm that records for individual local sensors are being collected regularly -- approximately every 5 minutes unless you've changed the sampling frequency.  And you should see no duplicated records for an individual sensor with timestamps differing by just a few seconds.  `WDL_433` de-duplicates the received JSON messages and spaces the timing of the database records it saves.  Spurious records from more remote sensors will also appear.
5.  If you can identify the `SensorID` of individual sensors and want to associate them with a familiar name (e.g., one in your office, another on your deck, and another at your neighbor's), note those SensorID's:
    *  in sqlite3, `SELECT DISTINCT SensorID from SensorData;`
    *  in MariaDB/MySQL: `use Weather; SELECT DISTINCT Sensorid from SensorData;`
    *  edit the file `WDL_433_Sensor_Aliases.ini` to associate your familiar name with the SensorID as rtl_433 receives it.
6.  When you are ready to put WDL_433 into production operation, type `sudo make install` to:
    *  copy the `WDL_433` program to a systems directory (by default, into `/usr/local/bin`)
    *  copy files `WDL_433.ini` and `WDL_433_Sensor_Alias.ini` into a systems directory (by default, into `/usr/local/etc`)
    *  copy `WDL_433.service` into `/etc/systemd/system/`), enable it as a system service, and start its operation.

### Uninstalling

To disable and remove WDL_433, connect to the `WS_433/WDL_433` directory and type `sudo make uninstall`.

## Detailed Installation Steps

### Configuring WDL_433 Operating Parameters

The file `WDL_433.h` documents parameters you might want to change to affect the routine operation of WDL_433, such as database identity or the frequency of collection of data from the sensors.

Though WDL_433 will compile and execute with parameters as they are set in the repository files, these are parameters in `WDL_433.h` that you might want to change before compiling WDL_433 to tailor it to your installation environment:
```
#define DBPATH  "/var/databases/"                 // Path to the sqlite3 database
#define DBNAME  "Weather"                         // SQL database name
#define DBTABLE "SensorData"                      // SQL table name
#define DUP_REC 2                                 // Minimum time in sec between non-duplicate records
#define recordingInterval 5*60                    // 5-minute minimum between sensor records
#define INI_PATH   ".:~:/usr/local/etc:/etc"      // search path for .ini & aliases
#define INI_FILE   APP_NAME".ini"                 // default name of .ini file
#define ALIAS_FILE APP_NAME"_Sensor_Aliases.ini"  // default name of alias file
```

Finally, edit the file `WDL_433.ini` to set any remaining parameters (most notably, the MySQL database hostname, username, and password if you're using MariaDB/MySQL).

### Database Choice

Use of sqlite3 has the advantage that WDL_433 creates the database file and table automatically (if permissions allow) and can be set up in the user's directory with no special privileges.  Use of MySQL requires that a server be installed and in operation, with appropriate permissions for the user to create a database and table.

#### sqlite3 Database Setup

The sqlite3 database path/filename is set by default to be `/var/databases/Weather.db`.  **Note that that file will normally be protected, so either WDL_433 must run as root (or as a systemd service) or the file must be created and protections set to enable writing by the user.**

On startup, if the sqlite3 database file `/var/databases/Weather.db` doesn't exist, WDL_433 creates it (if permissions allow; if this fails, check permissions and/or run as `sudo ,/WDL_433`).  The sqlite3 code uses a table named `SensorData` in that database file.  Again, if that table doesn't exist in the file, WDL_433 creates it with the command:
```
CREATE TABLE IF NOT EXISTS SensorData (date_time TEXT, sensorID TEXT,
temp1 REAL, temp2 REAL, rh REAL, press REAL, light REAL);
```

During operation, WDL_433 receives sensor data from the rtl_433 server as JSON packets, deserializes the data, and appends the received data to the sqlite3 database file with the command:
```
INSERT INTO SensorData (date_time, sensorID, temp1, temp2, rh, press, light)
VALUES ('%s', '%s', %5.1f, %5.1f, %3.0f, %6.1f, %3.0f);
and writing variables (date_time, sensorID, temp1, temp2, rh, press, light)
```
The sqlite3 database file is opened and then immediately closed when recording each individual sampling, so that the file is minimally vulnerable to corruption in case of system crash.

The sqlite3 database can be examined as a normal sqlite3 database table, for example, with the command:
```
$ sqlite3 /var/databases/Weather.db
SQLite version 3.40.1 2022-12-28 14:03:47
Enter ".help" for usage hints.
sqlite> .headers on
sqlite> .separator \t
sqlite> SELECT * from SensorData where datetime(date_time) > datetime('now', '-1 hours', 'localtime');
date_time            sensorID                   temp1	temp2	rh	   press	 light
2025-04-27 14:45:41	Neighbor                   14.9	0.0	47.0	0.0	 0.0
2025-04-27 14:46:21	Prologue-TH/41/2	         13.0	0.0	21.0	0.0	 0.0
2025-04-27 14:46:27	Acurite-Tower/9118/A	      11.2	0.0	52.0	0.0	 0.0
2025-04-27 14:46:27	SunRoom                    19.7	0.0	0.0	0.0	 0.0
2025-04-27 14:47:12	Deck                       13.2	0.0	40.0	0.0	 0.0
2025-04-27 14:48:42  Office                     21.0	22.0	47.0	1007.3 78.0
...
```

#### MySQL Database Setup

Operation of WDL_433 using a MySQL database is very similar to its operation using sqlite3.  However, the setup is a bit more complicated:

*  You must have a MariaDB/MySQL server that is accessible from the computer running WDL_433;
*  You must know the MySQL hostname and a username and password for a MySQL account on that server.

Start up WDL_433 from the `WS_433/WDL_433` directory with the command `./WDL_433 -d` to confirm that your parameters are set as you intended.  WDL_433 will check for the database and table and create them if necessary, and with `-d` on you'll see that process.  You'll see WDL_433 report the records as it appends them to the database.  After it has collected a few records, press \<Control-C\> to stop WDL_433 and then confirm that records have been stored in the MySQL database using the MySQL command (which shows the records collected over the past hour):
```
MariaDB [Weather]> select * from SensorData where UNIX_TIMESTAMP(date_time) > UNIX_TIMESTAMP()-1*60*60;
+---------------------+---------------------------+-------+-------+------+--------+-------+
| date_time           | sensorID                  | temp1 | temp2 | rh   | press  | light |
+---------------------+---------------------------+-------+-------+------+--------+-------+
| 2025-04-27 16:33:24 | SunRoom                   |    19 |     0 |    0 |      0 |     0 |
| 2025-04-27 16:33:24 | Office                    |  20.8 |  22.3 |   47 | 1008.1 |    79 |
| 2025-04-27 16:33:26 | Neighbor                  |  14.1 |     0 |   44 |      0 |     0 |
| 2025-04-27 16:33:36 | LaCrosse-TX141THBv2/221/0 |  12.1 |     0 |   48 |      0 |     0 |
| 2025-04-27 16:33:50 | Deck                      |    13 |     0 |   39 |      0 |     0 |
+---------------------+---------------------------+-------+-------+------+--------+-------+
```
If your command prints a table like the one above, you have a correctly-functioning WDL_433 MySQL setup!

Before you put it into production service, you will likely want to edit the `WDL_433.ini` file to provide your MySQL host parameters.  If you don't, you'll be prompted for their values each time you start WDL_433 (which won't work if you want to run WDL_433 as a systemd service -- you'll need to set up the `WDL_433.ini` file and set appropriate security restrictions to keep the MySQL database password private).

### Both SQL Options -- Production Mode

Whether you've chosen to run with sqlite3 or with MySQL, if you start WDL_433 with `./WDL_433`, it will now append the meterological data it receives from the rtl_433 JSON publications to the sqlite3 or MySQL table using an `INSERT` command string in the sqlite3 example above.  And, again, you can examine that data using sqlite3 or MySQL commands similar to the sqlite3 command examples given above.

If you `sudo make install` or (if using MySQL, `sudo make USE_MYSQL=1 install`, WDL_433 will begin autonomous operation as a systemd service to collect and archive meterological data to its SQL table, and it will restart automatically when you reboot your system.  If you're using MySQL, that install will protect the `WDL_433.ini` file from access by any user other than `root`, since that file contains the username and password to your MySQL server.

You can check that the data are being collected with a command such as:
```
$sqlite3 /var/databases/Weather.db
SQLite version 3.40.1 2022-12-28 14:03:47
Enter ".help" for usage hints.
sqlite> select * from SensorData where date_time > datetime('now', '-8 hours');
2025-04-27 14:02:58|TFA-303221/221/1|11.7|0.0|50.0|0.0|0.0
2025-04-27 14:03:04|GT-WT02/146/2|26.4|0.0|100.0|0.0|0.0
2025-04-27 14:03:04|Oregon-SL109H/41/2|52.9|0.0|52.0|0.0|0.0
2025-04-27 14:05:08|SunRoom|19.9|0.0|0.0|0.0|0.0
2025-04-27 14:05:29|Neighbor|15.4|0.0|45.0|0.0|0.0
2025-04-27 14:06:23|Office|21.5|22.3|47.0|1006.8|82.0
2025-04-27 14:06:36|Prologue-TH/41/2|14.1|0.0|20.0|0.0|0.0
...
```
For MySQL, the equivalent commands would be:
```
USE Weather;
MariaDB [Weather]> select * from SensorData where UNIX_TIMESTAMP(date_time) > UNIX_TIMESTAMP()-8*60*60;
```

You can manage your WDL_433 service as you would any other `systemd` service using the `systemctl` commands `stop`, `start`, `restart`, `disable`, `enable`.

In a relatively rural area, the sqlite3 database grows by about 0.4MB/day.  The `WDL_433.logrotate` file in the repository compresses the prior month's database file and starts a new one on a monthly basis.  A better approach would be to delete all but the last two weeks of the database file after compressing (see TO-DO's below).  But you might find the preliminary respository version useful.

## Uninstall

You can use `make clean` from the `WS_433/WDL_433` directory to remove construction debris from the `make` commands.  

To remove WDL_433 installation components and stop, disable, and remove WDL_433.service to prevent it from restarting upon a reboot, type `sudo make uninstall`.  

To remove the databases, use `sudo rm /var/databases/Weather.db` to remove the sqlite3 database or MySQL command `DROP DATABASE Weather;` to remove the MySQL database.

## TO-DO's

1. For sqlite3 database, create a logrotate file that:
   *  Is executed at month-end
   *  Compresses and backs up the past month's database file
   *  Deletes all records from the Weather.db file that are more than 2 weeks old.
2. Consider implementing subscription to rtl_433 HTTP streaming service as an alternative to MQTT service.

## Author

David Todd, hdtodd@gmail.com, 2025.04.27.
