#!/usr/bin/env python3
"""
    ========================================================
    Name: WeatherGraph.py
    --------------------------------------------------------
    Author:
    HDTodd based on work of prior authors,
    Most recently: Craig Deaton
    Edited 2025.04.21 for Python3 for use in WS_433/WWW_433
    --------------------------------------------------------
    Purpose:
       To query and retrieve data from Weather.db and graph
       for an interval period selected by the user
    --------------------------------------------------------
    Environment:
       Runs as a Python script and connects to a SQLite3 database
    --------------------------------------------------------
    Invocation:
       python3 WeatherData.py 
    --------------------------------------------------------
    Parameters:

    The global 'period' can be used to select the past number of hours to graph

    This script assumes the sqlite3 table SensorData was created with the command
        CREATE TABLE SensorData (date_time TEXT, sensorID TEXT, temp1 REAL,
            temp2 REAL, rh REAL, press REAL, light REAL);

    Then the fields selected for this script's table rows are:
    field# [0]                   [1]        [2]     [3]   [4]      [5]
           date_time           | sensorID | temp1 | rh  | press  | light
           2025-04-19 15:40:27 | Office   | 25.0  | 0.0 | 1010.0 | 79.0
   
    This version selects date_time, rh, and press for a specific sensorID
    for display on the graph

    --------------------------------------------------------
    Output:
       Generated HTML for title, form data, javascript for chart from Google Charts, and
       summary stats for min, max, avg temps for the selected period.  All displayed via browser
    --------------------------------------------------------
    Modifications:
    Version	Date		Name	Description
    ------------------------------------------------------------------------
    0001	1-Oct-2015	CD	Original script from https://github.com/Pyplate/rpi_temp_logger
    001	       11-Oct-2015	CD	modified script for this particular environment, pointed
                                          dbname to proper file name,
    002        23-Jan-2016      HDT     Modified for WeatherData 
    003        21-Apr-2025      HDT     Modified for WS_433/WWW_433 
    ========================================================
"""

import sqlite3
import sys
import socket

# global variables
sensor = 'Deck'
period = 10*24  # number of hours of historical data to present
dbname='/var/databases/Weather.db'

# ========================================================
# convert rows from database into a javascript table
def create_table(rows):
    chart_table=""

#   This is where the data fields are selected from the database: date_time, temp1, rh
    for row in rows:
        rowstr="['{0}', {1}, {2}],\n".format(str(row[0]), float(row[2]), float(row[4]))
        chart_table+=rowstr
    return chart_table

# ========================================================
def main():

    # get data from the database for the period specied y 
    # if you choose not to use the SQLite3 database, construct you record array in a similar manner to the output
    # from the cursor fetch all rows
    #      date-time string  temperature string \n 
    #          and then skip the checking for len(records) and doing the chart_table function,  just call the
    #  

    conn=sqlite3.connect(dbname)
    curs=conn.cursor()
    curs.execute("SELECT * FROM SensorData WHERE date_time>datetime('now','-%s hours') and SensorID='%s'" % (period, sensor))
    records=curs.fetchall()
    conn.close()

    # -------------------------------------
    # convert record rows to table if anyreturned
    # -------------------------------------

    if len(records) != 0:
        # convert the data into a table
        table=create_table(records)
    else:
        print("No data found")
        sys.stdout.flush()
        return

    # -------------------------------------
    # start printing the page
    # -------------------------------------
    print("<html>\n<center>")
 
    # -------------------------------------
    # print the HTML head section
    # -------------------------------------

    print("<h1 style=\"Font-Family:Arial\">" + socket.gethostname() + " Meteorological Data Web Site</h1>")
    print("<h2 style=\"Font-Family:Arial\">Current conditions at {0} for sensor '{1}'</br>".format(records[-1][0], records[-1][1]))
    print("<font color=\"red\">Temp: {0:.1f}°F<font> and <font color=\"blue\">RH = {1:.0f}%</font></h2>".format(1.8*records[-1][2]+32.0, records[-1][4]))
    print("<p>")
    print("  <head>")

    # -------------------------------------
    # format and print the graph
    # -------------------------------------

    # google chart snippet
    chart_code_pre="""
    <!--Load the AJAX API-->
    <script type="text/javascript" src="https://www.google.com/jsapi"></script>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>

    <script type="text/javascript">
      google.load("visualization", "1", {packages:["corechart"]});
      google.setOnLoadCallback(drawChart);
      function drawChart() {
        var data = new google.visualization.DataTable();
        data.addColumn("string","DateTime");
	data.addColumn("number","Temp1 (°F)");
	data.addColumn("number","RH (%)");
        data.addRows ( [
      """
    chart_code_post="""
     ] )
        var options = {
          title: 'Outside Temp and Humidity History',
	  series: {
	    0: {targetAxisIndex: 0, color:'red'},
	    1: {targetAxisIndex: 1, color:'blue'}
	  },
	  vAxes: {
	    0: {title: 'Temp1 (°F)'},
	    1: {title: 'RH (%)'}
	  }  
        };
        var chart = new google.visualization.LineChart(document.getElementById('chart_div'));
        chart.draw(data, options);
      }
    </script>
  </head>
  <body>
    <!--Div that will hold the line graph-->                                                                      <div id="chart_div" style="width: 900px; height: 500px;"></div>                                             </body>                                                                                                     </center>                                                                                                     </html>                                                                                                           """

    # ---------------------------------------
    # print the page body
    # ---------------------------------------

    print(chart_code_pre)
    print(table)
    print(chart_code_post)

    conn.close()

    sys.stdout.flush()

if __name__=="__main__":
    main()
