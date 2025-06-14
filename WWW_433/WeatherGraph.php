<!-- Template for a web site home page that also displays the history of
       WeatherStation-collected data as a graph
     Uses Google Charts for the display
     Written by HDTodd, January, 2016 borrowing heavily from numerous prior
        authors, particularly Craig Deaton
     Updated 2018.09.17 for Apache 2.4.25 and to use 
       Google AJAX JQuery API 3.3.1 (https://developers.google.com/speed/libraries/)
     Updated 2025.04.21 for the WS_433 system, to present data collected by WDL_433
       from an ISM-band remote-sensor rtl_433 server
-->
<?php
$HISTORY = "'-240 hours'";     //period of time over which to display temps
$DB_LOC  = "/var/databases/";  //location of the sqlite3 db
$DB_NAME = "Weather.db";       //name of sqlite3 db
$SENSOR  = "Deck";             //sensor to report
$DATA1   = "temp1";            //first data field to report
$DATA2   = "rh";               //second data field to report

/*    This script assumes the sqlite3 table SensorData was created with the command
        CREATE TABLE SensorData (date_time TEXT, sensorID TEXT, temp1 REAL,
            temp2 REAL, rh REAL, press REAL, light REAL);

      Then the fields selected for this script's table rows are:
      field# [0]                   [1]        [2]     [3]   [4]      [5]
             date_time           | sensorID | temp1 | rh  | press  | light
             2025-04-19 15:40:27 | Office   | 25.0  | 0.0 | 1010.0 | 79.0

      This version selects date_time, temp1, and rh for a specific sensorID
      for display on the graph

     First, PHP code to populate an array with the [time,temp,rh] data
     and create a JSON array for the Javascript below
*/
$db = new PDO('sqlite:' . $DB_LOC . $DB_NAME) 
      	  or die('Cannot open database ' . $DB_NAME);
$query = "SELECT date_time, temp1, rh FROM SensorData  WHERE sensorID='$SENSOR' AND date_time>datetime('now',$HISTORY)"; 
foreach ($db->query($query) as $row)
   $chart_array[]=array( (string)$row['date_time'], 1.8*(float)$row['temp1']+32.0, (float)$row['rh']); 

//  Now get the latest entry to report as "current readings"
$query = "SELECT date_time, temp1, rh FROM SensorData WHERE sensorID='$SENSOR' ORDER BY date_time  DESC LIMIT 1";
foreach ($db->query($query) as $row) {
  $last_time=(string)$row['date_time'];
  $last_temp1=json_encode( 1.8*(float)$row['temp1']+32.0);
  $last_rh=json_encode( (float)$row['rh']);
}

//  Convert the table to a JSON array for the Javascript
$temp_data=json_encode($chart_array);
?>

<!-- Here's the HTML code for the site, followed by the JS component for the chart
     Use Google Charts javascript to generate the graphs
-->
<html>
<center style="font-family:Arial">
<h1 style="font-family:Arial">The <?php echo gethostname() ?> Meterological Data Web Site</h1>
<h2 style="font-family:Arial">Current conditions at <?php echo $last_time ?> for sensor '<?php echo $SENSOR ?>'</br> 
<font color="red" >Temp: <?php echo round($last_temp1,1,PHP_ROUND_HALF_UP) ?>°F </font>and 
<font color="blue">RH = <?php echo $last_rh ?>% </font></h2>
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
	data.addColumn("number","Temp1 (°F)");
	data.addColumn("number","RH (%)");
        data.addRows( <?php echo $temp_data ?>);
        var options = {
          title: 'Outside Temp and Humidity History',
	  series: {
	    0: {targetAxisIndex: 0, color: 'red'},
	    1: {targetAxisIndex: 1, color: 'blue'}
	  },
	  vAxes: {
	    0: {title: 'Temp (°F)'},
	    1: {title: 'RH (%)'}
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
