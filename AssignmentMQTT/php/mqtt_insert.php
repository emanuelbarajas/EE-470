
<?php
// --------------------------------------------------
// EE 470 - MQTT Database Insert Endpoint
// --------------------------------------------------

header("Content-Type: application/json");

function respond($code, $data) {
    http_response_code($code);
    echo json_encode($data);
    exit;
}

// --------------------------------------------------
// Database Configuration
// --------------------------------------------------

$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "myPassword";

$apiKey = "d7fbe49aaab527f1471a091cbc0ea8c199f8a8692fd3eb84dc7b865e8b37b35b";

// --------------------------------------------------
// Require POST and Authenticate
// --------------------------------------------------

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    header("Allow: POST");
    respond(405, ["error" => "Method not allowed"]);
}

$providedKey = $_SERVER["HTTP_X_API_KEY"] ?? "";

if (!hash_equals($apiKey, $providedKey)) {
    respond(401, ["error" => "Unauthorized"]);
}

// --------------------------------------------------
// Decode JSON
// --------------------------------------------------

$data = json_decode(
    file_get_contents("php://input"),
    true
);

if (!is_array($data)) {
    respond(400, ["error" => "Invalid JSON"]);
}

$type = $data["type"] ?? "potentiometer";

// --------------------------------------------------
// Validate Potentiometer Data
// --------------------------------------------------

if ($type === "potentiometer") {

    $node = $data["node_name"] ?? null;
    $voltage = $data["voltage"] ?? null;

    if (
        $node !== "emanuel470" ||
        !is_numeric($voltage) ||
        !is_scalar($voltage) ||
        !is_finite((float)$voltage) ||
        (float)$voltage < 0 ||
        (float)$voltage > 3.3
    ) {
        respond(400, [
            "error" => "Invalid potentiometer measurement"
        ]);
    }

    $voltage = (float)$voltage;
}

// --------------------------------------------------
// Validate LED Status
// --------------------------------------------------

elseif ($type === "led_status") {

    $ledName = $data["led_name"] ?? null;
    $state = $data["state"] ?? null;

    if (
        !in_array($ledName, ["emanuel", "antonio"], true) ||
        !is_int($state) ||
        !in_array($state, [0, 1], true)
    ) {
        respond(400, [
            "error" => "Invalid LED status"
        ]);
    }
}

else {
    respond(400, ["error" => "Unknown data type"]);
}

// --------------------------------------------------
// Database Operations
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

    // Store timestamps in UTC
    $conn->query("SET time_zone = '+00:00'");

    // -----------------------------------------
    // Insert Potentiometer Measurement
    // -----------------------------------------

    if ($type === "potentiometer") {

        $stmt = $conn->prepare(
            "INSERT INTO mqtt_potentiometer
             (node_name, voltage)
             VALUES (?, ?)"
        );

        $stmt->bind_param(
            "sd",
            $node,
            $voltage
        );

        $stmt->execute();

        $insertId = $conn->insert_id;

        $stmt->close();

        $result = [
            "status" => "success",
            "message" => "Measurement stored",
            "id" => $insertId,
            "node_name" => $node,
            "voltage" => $voltage
        ];
    }

    // -----------------------------------------
    // Update LED Status
    // -----------------------------------------

    else {

        $stmt = $conn->prepare(
            "INSERT INTO mqtt_led_status
             (led_name, state)
             VALUES (?, ?)
             ON DUPLICATE KEY UPDATE
                state = VALUES(state),
                time_updated = CURRENT_TIMESTAMP"
        );

        $stmt->bind_param(
            "si",
            $ledName,
            $state
        );

        $stmt->execute();
        $stmt->close();

        $result = [
            "status" => "success",
            "message" => "LED status updated",
            "led_name" => $ledName,
            "state" => $state
        ];
    }

    $conn->close();

    respond(201, $result);

} catch (mysqli_sql_exception $e) {

    error_log($e->getMessage());

    respond(500, [
        "error" => "Database operation failed"
    ]);
}
?>
