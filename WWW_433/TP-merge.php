<?php
/*
    Template for a web page that displays the history of
	WeatherStation-collected data as a graph

    This script assumes the sqlite3 table SensorData was created with the command
	CREATE TABLE SensorData (date_time TEXT, sensorID TEXT, temp1 REAL,
	    temp2 REAL, rh REAL, press REAL, light REAL);

    Then the fields selected for this script's table rows are:
        field# [0]             [1]        [2]    [3]   [4]     [5]   |  [6]
        date_time           | sensorID | temp1 |temp2 | rh  | press  | light
        2025-04-19 15:40:27 | Office   | 25.0  | 0.0 | 1010.0 | 79.0

      This version selects date_time and temp1 (outdoor temp) from one sensor,
      and barometric pressure from a second sensor, categorizes the times into
      n-minute ($BIN) intervals, and displays the integrated readings on the graph.
      Temperature and pressure readings are synchronized to within $BIN seconds.
      *** To function correctly, both sensors should be reporting at least
	  every ($BIN) seconds ***

    Uses Google Charts for the display
    Written by HDTodd, January, 2016 borrowing heavily from numerous prior
	authors, particularly Craig Deaton
    Updated 2018.09.17 for Apache 2.4.25 and to use 
	Google AJAX JQuery API 3.3.1 (https://developers.google.com/speed/libraries/)
    Updated 2025.04.21 for the WS_433 system, to present data collected by WDL_433
	from an ISM-band remote-sensor rtl_433 server
    Amended 2025.6.24 to demonstrate merge of data from two sensors
*/

$HISTORY = "'-240 hours'";     //period of time over which to display temps
$BIN     = 5*60;               //granularity of time resolution for graph in sec
$DB_LOC  = "/var/databases/";  //location of the sqlite3 db
$DB_NAME = "Weather.db";       //name of sqlite3 db
$SENSOR1 = "Deck";             //sensor source for date_time and temp
$SENSOR2 = "Desk";	       //sensor source for pressure
$DATA1   = "temp1";            //field name for outside temp from primary sensor
$DATA2   = "press";            //field name for barometric pressure from secondary

//     First, PHP code to populate an array with the [time,temp,press] data
//     and create a JSON array for the Javascript below

$db = new PDO('sqlite:' . $DB_LOC . $DB_NAME) 
      	  or die('Cannot open database ' . $DB_NAME);

// Create temporary table "wd" to hold the merged data, populate the table,
//     and merge the readings
$stmts = [
          "CREATE TEMPORARY TABLE IF NOT EXISTS wd (dt TEXT, temp REAL, press REAL)",
	  "DELETE FROM wd",
	  "INSERT INTO wd SELECT (unixepoch(date_time)/$BIN), temp1, 0. " .
	      "FROM SensorData WHERE sensorID='Deck' AND " .
	      "date_time > datetime('now', $HISTORY, 'localtime') ORDER BY date_time",
	  "UPDATE wd SET press = s.press FROM (SELECT date_time, sensorID, press " .
	      "FROM SensorData) as s WHERE wd.dt=(unixepoch(s.date_time)/$BIN) " .
	      "AND s.sensorID='Desk'",
	  "DELETE FROM wd WHERE press=0",
	  "UPDATE wd set dt=datetime(dt*$BIN, 'unixepoch')"
	  ];
	  
foreach($stmts as $stmt)
    $db->exec($stmt);

//  Create the chart data array
$query = "SELECT dt, temp, press from wd";
foreach ($db->query($query) as $row)
    $chart_array[]=array( (string)$row['dt'],
        round(1.8*(float)$row['temp']+32.0,1), (float)$row['press']); 

//  Now get the latest entry to report as "current readings"
$query = "SELECT dt, temp, press FROM wd ORDER BY dt DESC LIMIT 1";
foreach ($db->query($query) as $row) {
  $last_time=(string)$row['dt'];
  $last_temp=json_encode( round(1.8*(float)$row['temp']+32.0), 1);
  $last_press=json_encode( (float)$row['press']);
}

//  Convert the table to a JSON array for the Javascript
$temp_data=json_encode($chart_array);

//  Now generate the html code for the graph, with the JS component for the chart
//  Use Google Charts javascript to generate the graph
?>
<html>
<center style="font-family:Arial">
<h1 style="font-family:Arial">The <?php echo gethostname() ?> Meterological Data Web Site</h1>
<h2 style="font-family:Arial">Current conditions at <?php echo $last_time ?> for sensor '<?php echo $SENSOR1 ?>'</br> 
<font color="red" >Temp: <?php echo round($last_temp,1,PHP_ROUND_HALF_UP) ?>°F </font>and 
<font color="blue">Pressure = <?php echo $last_press ?>hPa </font></h2>
<p style="font-family:Arial">
  <head>
    <!--Load the AJAX API-->
    <script type="text/javascript" src="https://www.google.com/jsapi"></script>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>

    <script type="text/javascript">
      google.load("visualization", "1", {packages:["corechart"]});
      google.setOnLoadCallback(drawChart);
      function drawChart() {
        var data = new google.visualization.DataTable();
        data.addColumn("string","DateTime");
	data.addColumn("number","Temp(°F)");
	data.addColumn("number","Press(hPa)");
        data.addRows( <?php echo $temp_data ?>);
        var options = {
          title: 'Outside Temp and Pressure History',
	  series: {
	    0: {targetAxisIndex: 0, color: 'red'},
	    1: {targetAxisIndex: 1, color: 'blue'}
	  },
	  vAxes: {
	    0: {title: 'Temp (°F)'},
	    1: {title: 'Press (hPa)'}
	  }  
        };
        var chart = new google.visualization.LineChart(document.getElementById('chart_div'));
        chart.draw(data, options);
      }
    </script>
  </head>
  <body>
    <!--Div that will hold the line graph-->
    <div id="chart_div" style="width: 900px; height: 500px;"></div>
  </body>
</center>
</html>
