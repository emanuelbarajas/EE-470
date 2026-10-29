
<?php
$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "MyPassword";

$conn = new mysqli($host, $user, $pass, $db);

if ($conn->connect_error) {
    die("Database connection failed");
}

$register = $conn->query("SELECT * FROM sensor_register ORDER BY node_name");
$data = $conn->query("SELECT * FROM sensor_data ORDER BY node_name, time_received");
$nodes = $conn->query("SELECT node_name FROM sensor_register ORDER BY node_name");

if (!$register || !$data || !$nodes) {
    die("Database query failed");
}

function showTable($result) {
    echo "<table border='1' cellpadding='8' cellspacing='0'>";

    echo "<tr>";
    foreach ($result->fetch_fields() as $field) {
        echo "<th>" . htmlspecialchars($field->name) . "</th>";
    }
    echo "</tr>";

    while ($row = $result->fetch_assoc()) {
        echo "<tr>";
        foreach ($row as $value) {
            echo "<td>" . htmlspecialchars((string)$value) . "</td>";
        }
        echo "</tr>";
    }

    echo "</table>";
}
?>

<!DOCTYPE html>
<html>
<head>
    <title>EE 470 Sensor Data</title>

    <style>
        body { font-family: Arial; margin: 30px; }
        table { border-collapse: collapse; margin-bottom: 30px; }
        th { background: #117a72; color: white; }
        td { background: #f5f5f5; }
        #chart-container { max-width: 800px; margin-top: 30px; }
        select { padding: 8px; font-size: 16px; }
        #stats { line-height: 1.8; }
    </style>
</head>

<body>

<h1>EE 470 Sensor Database</h1>

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

<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.6.0/jquery.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.1/chart.min.js"></script>
<script src="chartjs/app.js?v=5"></script>

</body>
</html>

<?php $conn->close(); ?>
