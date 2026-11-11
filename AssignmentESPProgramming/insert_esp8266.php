
<?php
/*
 * EE 470 ESP8266 Sensor Receiver
 * File: insert_esp8266.php
 * Version: 1.0
 *
 * Receives ESP8266 measurements using POST.
 * Validates node, timestamp, light, and distance.
 * Stores accepted readings in sensor_data.
 * Returns JSON confirmation.
 */

header("Content-Type: application/json");

$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "@Redsonguy1";

function respond($code, $data) {
    http_response_code($code);
    echo json_encode($data);
    exit;
}

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    respond(405, [
        "status" => "error",
        "message" => "POST required"
    ]);
}

// Read incoming values
$node = $_POST["node_name"] ?? "";
$time = $_POST["time_received"] ?? "";
$light = $_POST["light_voltage"] ?? "";
$distance = $_POST["distance_cm"] ?? "";

// Validate node format
if (!is_string($node) ||
    !preg_match('/^node_[1-9][0-9]*$/', $node) ||
    strlen($node) > 10) {

    respond(400, [
        "status" => "error",
        "message" => "Invalid node"
    ]);
}

// Require timestamp from ESP
if (!is_string($time)) {
    respond(400, [
        "status" => "error",
        "message" => "Invalid timestamp"
    ]);
}

$date = DateTimeImmutable::createFromFormat(
    "!Y-m-d H:i:s",
    $time
);

if (!$date ||
    $date->format("Y-m-d H:i:s") !== $time) {

    respond(400, [
        "status" => "error",
        "message" => "Timestamp required or invalid"
    ]);
}

// Validate measurements
if (!is_scalar($light) ||
    !is_scalar($distance) ||
    !is_numeric($light) ||
    !is_numeric($distance) ||
    $light < 0 ||
    $light > 3.3 ||
    $distance < 2 ||
    $distance > 400) {

    respond(400, [
        "status" => "error",
        "message" => "Sensor values out of range"
    ]);
}

try {
    $conn = new mysqli(
        $host,
        $user,
        $pass,
        $db
    );

    $conn->set_charset("utf8mb4");

    // Check node registration
    $check = $conn->prepare(
        "SELECT node_name
         FROM sensor_register
         WHERE node_name = ?"
    );

    $check->bind_param("s", $node);
    $check->execute();

    if ($check->get_result()->num_rows === 0) {
        respond(400, [
            "status" => "error",
            "message" => "Node not registered"
        ]);
    }

    $check->close();

    // Check if this reading already exists
    $check = $conn->prepare(
        "SELECT light_voltage, distance_cm
         FROM sensor_data
         WHERE node_name = ?
         AND time_received = ?"
    );

    $check->bind_param("ss", $node, $time);
    $check->execute();

    $existing = $check->get_result()->fetch_assoc();
    $check->close();

    if ($existing !== null) {
        if ($existing["light_voltage"] !== null &&
            $existing["distance_cm"] !== null &&
            abs((float)$existing["light_voltage"] - (float)$light) < 0.0005 &&
            abs((float)$existing["distance_cm"] - (float)$distance) < 0.005) {

            respond(200, [
                "status" => "duplicate",
                "message" => "Measurement already stored"
            ]);
        }

        respond(409, [
            "status" => "error",
            "message" => "Timestamp already used for different data"
        ]);
    }

    // Insert new reading
    $stmt = $conn->prepare(
        "INSERT INTO sensor_data
         (node_name, time_received,
          light_voltage, distance_cm)
         VALUES (?, ?, ?, ?)"
    );

    $stmt->bind_param(
        "ssdd",
        $node,
        $time,
        $light,
        $distance
    );

    $stmt->execute();
    $stmt->close();

    // sensor_activity trigger updates the count
    respond(201, [
        "status" => "success",
        "message" => "Measurement stored",
        "node_name" => $node,
        "time_received" => $time,
        "light_voltage" => (float)$light,
        "distance_cm" => (float)$distance
    ]);

} catch (mysqli_sql_exception $e) {
    error_log($e->getMessage());

    respond(500, [
        "status" => "error",
        "message" => "Database operation failed"
    ]);
}
?>
