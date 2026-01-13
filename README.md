# WS_433: An rtl_433-based WeatherStation

## Description

WDL_433 monitors an rtl_433 server feed for weather-related JSON packets published via MQTT and	records	specific fields	from those JSON packets into an SQL database table.  WWW_433 is a set of Apache2 server tools that can be used to display selected data from that sqlite3 or MariaDB/MySQL SQL database table in graphical form via a web browser. Those are the two components of WS_433. WS_433 is a revision of the earlier Arduno-based [WeatherStation](http://github.com/hdtodd/WeatherStation) system.  This revised version collects weather data from an ISM-band rtl_433 server rather than from a directly-attached Arduino Uno.  

<img width="836" alt="WS_433" src="https://github.com/user-attachments/assets/85252aaf-878e-4b0e-b426-b29cc3131578" />


This version of WDL_433, as distributed in this repository, archives to an SQL database the following data as received by the rtl_433 server:
*  A date-time stamp that records the time the sensor reading was processed by the rtl_433 server;
*  The name of the sensor that sent the data (in the format 'model/id/channel' as reported by rtl_433);
*  The reading of 'temperature_C';
*  The reading of 'temperature_C_2';
*  The reading of 'humidity';
*  The reading of 'pressure_hPa';
*  The reading of 'Light %'.

Data values are recorded as "float" values and are set to 0.0 if the packet does not contain a field with that label.  The duplicated messages in the packet received by rtl_433 are de-duplicated, so that only one database entry is made for each reading.  Readings from an individual sensor are recorded no more often than every 5 minutes (default setting)

```
MariaDB [Weather]> select * from SensorData;
+---------------------+---------------------------+-------+-------+------+--------+-------+
| date_time           | sensorID                  | temp1 | temp2 | rh   | press  | light |
+---------------------+---------------------------+-------+-------+------+--------+-------+
| 2025-04-29 22:12:23 | Neighbor                  |  15.7 |     0 |   60 |      0 |     0 |
| 2025-04-29 22:12:27 | Office                    |  16.3 |  22.3 |   48 | 1011.9 |    47 |
| 2025-04-29 22:12:35 | SunRoom                   |  20.9 |     0 |    0 |      0 |     0 |
| 2025-04-29 22:12:38 | Deck                      |  15.4 |     0 |   59 |      0 |     0 |
...
```

The PHP-based web page displays temperature and relative humidity for a period of 10 days (default setting) for a specific sensor ('Deck' in the example).  The PHP code can be easily modified to select other values from other sensors from the database for display.  An equivalent Python version can be incorporated to insert the graphs in other web pages.

See the "Principles of Operation" manual, `doc/WS-PO.md`, for details and guidance on how to record other or additional readings.

This system was developed on a Raspberry Pi 4B running Raspberry Pi OS.  WDL_433 can be run on any computer that has access to the rtl_433 server via network connection.  It can be run as a `systemd` service (`.service` file included). Similarly, WWW_433 can be run on any computer with Apache2 and php installed and that has access to the weather database archived by WDL_433.

## Requirements

1.  An rtl_433 server configured to publish ISM-band remote-sensor broadcasts as MQTT JSON packets.  See [rtl_433](https://github.com/merbanan/rtl_433);
2.  For WDL_433: a computer with network access to that rtl_433 server and with either sqlite3 installed or with access to a MariaDB/MySQL server;
3.  For WWW_433: a computer with Apache2 and php installed and access to the SQL database set up for use by WDL_433.

## Quickstart Installation

1.  Configure the rtl_433 server to publish, via MQTT, in JSON format, the ISM remote-sensor packets it receives.  Note the MQTT publisher host name and topic (and port if not the default 1883).
2.  If you intend to use sqlite3 as your database repository, install sqlite3 and libsqlite3-dev on that computer; if you intend to use MariaDB/MySQL as your database repository, install mariadb-client and libmariadb-dev.  If you don't have a MySQL database server already available on your network, you'll find it easier to install and use sqlite3 to get started.
3.  Ensure that the computer has network access to that MQTT service.  Install mosquitto-client and mosquitto-dev.  Confirm that MQTT service is received on that computer by issuing the command `mosquitto_sub -h <host> -t "<topic>"`: you should see the JSON packets as they are being published from the rtl_433 server.
4.  Clone this repository to that computer.  Connect to the WDL_433 directory of the cloned repository.
5.  Type `make` to use sqlite3 as your database for archiving data; type `make USE_MYSQL=1` to use MariaDB/MySQL as your archival database.
6.  Edit the file `WDL_433.ini` to provide the parameters for your installation.  In particular, provide the MySQL hostname, username, and password if you're using MariaDB/MySQL.
7.  Test the compiled program by giving the command `./WDL_433 -d` (for debugging information).  WDL_433 will create the database and table if necessary.  Correct any directory or login permissions until WDL_433 begins correctly receiving and recording rtl_433 packets.
8.  If you know the SensorIDs ('model'/'id'/'channel') of your sensors, edit the [aliases] section of the file `WDL_433.ini` to associate each of them with an alias name that might be more familiar (e.g., `Acurite-609TXC/46/ = Deck` records readings from that Acurite sensor in the database as having originated from the sensor located on your "Deck").
9.  By default, the components for WDL_433 are installed under `/usr/local`.  Edit the Makefile to change that if you want to install to another root directory.  Then type `sudo make install` or `sudo make USE_MYSQL=1 install`.  That copies the necessary files to the appropriate places, installs a `.service` file for `systemd`, and enables and starts the service.
10.  Refer to `./doc/ WWW_433-Install.md` for guidance on testing and installing the graphical web page.

### Detailed Installation Instructions

Refer to the `./doc/` directory for detailed instructions on the installation of the various components.

## Release History

| Version | Date       | Changes |
|---------|------------|---------|
| v1.1    | 2025.06.30 | Improve MQTT server connection error reporting; make reconnecting to disconnectd server robust |
| v1.0    | 2025.06.27 | Integrated alias name assignment into .ini file processing |
| v0.1    | 2025.04.17 | Preliminary but fully-functional version. |

## Author

Written by David Todd, HDTodd@gmail.com as an integration of the prior [WeatherStation](http://github.com/hdtodd/WeatherStation) repository with [rtl_watch](http://github.com/hdtodd/rtl_watch) for use with `rtl_433` sensor data collection.  

