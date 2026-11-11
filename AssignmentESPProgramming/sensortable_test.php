
<?php
/*
 * EE 470 Sensor Database
 *
 * Assignment 4:
 * Temperature and humidity monitoring
 *
 * ESP8266 Assignment:
 * Light voltage and ultrasonic distance monitoring
 *
 * Both assignments share the same database.
 */

$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "@Redsonguy1";

$conn = new mysqli($host, $user, $pass, $db);

if ($conn->connect_error) {
    die("Database connection failed");
}

$conn->set_charset("utf8mb4");

// Existing Assignment 4 queries
$register = $conn->query(
    "SELECT * FROM sensor_register ORDER BY node_name"
);

$data = $conn->query(
    "SELECT * FROM sensor_data ORDER BY node_name, time_received"
);

$nodes = $conn->query(
    "SELECT node_name FROM sensor_register ORDER BY node_name"
);

// ESP8266 activity counters
$activity = $conn->query(
    "SELECT node_name, message_count
     FROM sensor_activity
     ORDER BY node_name"
);

// ESP8266 statistics
$espStats = $conn->query(
    "SELECT
        node_name,
        COUNT(*) AS total_readings,
        ROUND(AVG(light_voltage), 3) AS avg_light,
        MIN(light_voltage) AS min_light,
        MAX(light_voltage) AS max_light,
        ROUND(AVG(distance_cm), 2) AS avg_distance,
        MIN(distance_cm) AS min_distance,
        MAX(distance_cm) AS max_distance
     FROM sensor_data
     WHERE light_voltage IS NOT NULL
       AND distance_cm IS NOT NULL
     GROUP BY node_name
     ORDER BY node_name"
);

// ESP8266 measurements
$espData = $conn->query(
    "SELECT
        node_name,
        time_received,
        light_voltage,
        distance_cm
     FROM sensor_data
     WHERE light_voltage IS NOT NULL
       AND distance_cm IS NOT NULL
     ORDER BY node_name, time_received"
);

if (
    !$register ||
    !$data ||
    !$nodes ||
    !$activity ||
    !$espStats ||
    !$espData
) {
    die("Database query failed");
}

// Reusable table function
function showTable($result) {

    echo "<table border='1' cellpadding='8' cellspacing='0'>";

    echo "<tr>";

    foreach ($result->fetch_fields() as $field) {
        echo "<th>" .
            htmlspecialchars($field->name) .
            "</th>";
    }

    echo "</tr>";

    while ($row = $result->fetch_assoc()) {

        echo "<tr>";

        foreach ($row as $value) {

            echo "<td>" .
                htmlspecialchars((string)($value ?? "")) .
                "</td>";
        }

        echo "</tr>";
    }

    echo "</table>";
}

// Prepare ESP8266 chart data
$espChartData = [];

while ($row = $espData->fetch_assoc()) {

    $node = $row["node_name"];

    if (!isset($espChartData[$node])) {
        $espChartData[$node] = [];
    }

    $espChartData[$node][] = [
        "time" => $row["time_received"],
        "light" => (float)$row["light_voltage"],
        "distance" => (float)$row["distance_cm"]
    ];
}
?>

<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>EE 470 Sensor Data</title>

<style>
    body {
        font-family: Arial;
        margin: 30px;
    }

    table {
        border-collapse: collapse;
        margin-bottom: 30px;
    }

    th {
        background: #117a72;
        color: white;
    }

    td {
        background: #f5f5f5;
    }

    #chart-container {
        max-width: 800px;
        margin-top: 30px;
    }

    select {
        padding: 8px;
        font-size: 16px;
    }

    #stats {
        line-height: 1.8;
    }

    .esp-chart-container {
        max-width: 800px;
        height: 350px;
        margin-top: 30px;
        margin-bottom: 30px;
    }
</style>

</head>

<body>

<h1>EE 470 Sensor Database</h1>

<!-- ========================================== -->
<!-- Existing Assignment 4 -->
<!-- ========================================== -->

<h2>Registered Sensor Nodes</h2>
<?php showTable($register); ?>

<h2>Sensor Data Received</h2>
<?php showTable($data); ?>

<h2>Select Sensor Node</h2>

<select id="nodeSelect">

    <?php while ($row = $nodes->fetch_assoc()): ?>

        <option value="<?= htmlspecialchars($row['node_name']) ?>">
            <?= htmlspecialchars($row['node_name']) ?>
        </option>

    <?php endwhile; ?>

</select>

<h2>Sensor Statistics</h2>
<div id="stats"></div>

<h2>Temperature Graph</h2>

<div id="chart-container">
    <canvas id="mycanvas"></canvas>
</div>

<!-- ========================================== -->
<!-- ESP8266 Assignment -->
<!-- ========================================== -->

<hr>

<h1>ESP8266 Sensor Monitoring</h1>

<h2>Sensor Activity</h2>
<?php showTable($activity); ?>

<h2>ESP8266 Sensor Statistics</h2>
<?php showTable($espStats); ?>

<h2>Select ESP8266 Node</h2>

<select id="espNodeSelect">

    <?php foreach (array_keys($espChartData) as $node): ?>

        <option value="<?= htmlspecialchars($node) ?>">
            <?= htmlspecialchars($node) ?>
        </option>

    <?php endforeach; ?>

</select>

<h2>Light Voltage Graph</h2>

<div class="esp-chart-container">
    <canvas id="lightChart"></canvas>
</div>

<h2>Ultrasonic Distance Graph</h2>

<div class="esp-chart-container">
    <canvas id="distanceChart"></canvas>
</div>

<!-- ========================================== -->
<!-- JavaScript -->
<!-- ========================================== -->

<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.6.0/jquery.min.js"></script>

<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.1/chart.min.js"></script>

<!-- Original Assignment 4 functionality -->
<script src="chartjs/app.js?v=7"></script>

<script>

// ESP8266 measurements grouped by node
const espData = <?= json_encode(
    $espChartData,
    JSON_HEX_TAG |
    JSON_HEX_AMP |
    JSON_HEX_APOS |
    JSON_HEX_QUOT
) ?>;

let lightChart = null;
let distanceChart = null;

// Update ESP8266 graphs
function updateESPCharts() {

    const selectedNode =
        document.getElementById("espNodeSelect").value;

    const readings = espData[selectedNode] || [];

    const labels = readings.map(row => row.time);
    const light = readings.map(row => row.light);
    const distance = readings.map(row => row.distance);

    if (lightChart) {
        lightChart.destroy();
    }

    if (distanceChart) {
        distanceChart.destroy();
    }

    lightChart = new Chart(
        document.getElementById("lightChart"),
        {
            type: "line",
            data: {
                labels: labels,
                datasets: [{
                    label: "Light Voltage (V)",
                    data: light,
                    borderColor: "#117a72",
                    backgroundColor: "rgba(17,122,114,0.15)",
                    fill: true,
                    tension: 0.2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        title: {
                            display: true,
                            text: "Voltage (V)"
                        }
                    }
                }
            }
        }
    );

    distanceChart = new Chart(
        document.getElementById("distanceChart"),
        {
            type: "line",
            data: {
                labels: labels,
                datasets: [{
                    label: "Distance (cm)",
                    data: distance,
                    borderColor: "#117a72",
                    backgroundColor: "rgba(17,122,114,0.15)",
                    fill: true,
                    tension: 0.2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        title: {
                            display: true,
                            text: "Distance (cm)"
                        }
                    }
                }
            }
        }
    );
}

// Node selection
document.getElementById("espNodeSelect")
    .addEventListener("change", updateESPCharts);

// Initialize charts
updateESPCharts();

</script>

</body>
</html>

<?php $conn->close(); ?>
