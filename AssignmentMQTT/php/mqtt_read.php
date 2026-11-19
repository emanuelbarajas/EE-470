
<?php
// --------------------------------------------------
// EE 470 - MQTT Measurement and LED Reader
// --------------------------------------------------

header("Content-Type: application/json");
header("Cache-Control: no-store");

// --------------------------------------------------
// Database Configuration
// --------------------------------------------------

$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "@Redsonguy1";

// --------------------------------------------------
// Database Connection
// --------------------------------------------------

try {

    mysqli_report(
        MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT
    );

    $conn = new mysqli(
        $host,
        $user,
        $pass,
        $db
    );

    $conn->set_charset("utf8mb4");

    $conn->query("SET time_zone = '+00:00'");

    // --------------------------------------------------
    // Retrieve Latest 100 Potentiometer Measurements
    // --------------------------------------------------

    $sql = "
        SELECT
            id,
            node_name,
            voltage,
            DATE_FORMAT(
                time_received,
                '%Y-%m-%dT%H:%i:%sZ'
            ) AS time_received
        FROM mqtt_potentiometer
        ORDER BY id DESC
        LIMIT 100
    ";

    $result = $conn->query($sql);

    $measurements = $result->fetch_all(
        MYSQLI_ASSOC
    );

    $measurements = array_reverse(
        $measurements
    );

    // --------------------------------------------------
    // Retrieve Latest LED Statuses
    // --------------------------------------------------

    $sql = "
        SELECT
            led_name,
            state,
            DATE_FORMAT(
                time_updated,
                '%Y-%m-%dT%H:%i:%sZ'
            ) AS time_updated
        FROM mqtt_led_status
        WHERE led_name IN ('emanuel', 'antonio')
    ";

    $result = $conn->query($sql);

    $ledRows = $result->fetch_all(
        MYSQLI_ASSOC
    );

    $ledStatuses = [
        "emanuel" => null,
        "antonio" => null
    ];

    foreach ($ledRows as $row) {

        $ledStatuses[$row["led_name"]] = [
            "state" => (int)$row["state"],
            "time_updated" => $row["time_updated"]
        ];
    }

    // --------------------------------------------------
    // Return Combined JSON
    // --------------------------------------------------

    echo json_encode([
        "measurements" => $measurements,
        "led_status" => $ledStatuses
    ]);

    $conn->close();

} catch (mysqli_sql_exception $e) {

    error_log($e->getMessage());

    http_response_code(500);

    echo json_encode([
        "error" => "Unable to retrieve MQTT data"
    ]);
}
?>
