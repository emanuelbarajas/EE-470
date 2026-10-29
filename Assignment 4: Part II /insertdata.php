
<?php
// Database connection
$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "MyPassword";

header("Content-Type: application/json");

function respond($code, $data) {
    http_response_code($code);
    echo json_encode($data, JSON_PRETTY_PRINT);
    exit;
}

// Convert timestamp to MariaDB format
function formatTime($time) {
    if ($time === '') return null;

    foreach (['Y-m-d H:i:s', 'n-j-Y H:i:s'] as $format) {
        $date = DateTimeImmutable::createFromFormat(
            '!' . $format, $time
        );

        if ($date && $date->format($format) === $time) {
            return $date->format('Y-m-d H:i:s');
        }
    }

    return false;
}

try {
    $conn = new mysqli($host, $user, $pass, $db);
    $conn->set_charset("utf8mb4");

    $method = $_SERVER['REQUEST_METHOD'];
    $isBase64 = false;

    // GET without parameters: JSON for Chart.js
    if ($method === 'GET' && empty($_SERVER['QUERY_STRING'])) {
        $result = $conn->query(
            "SELECT node_name, time_received, temperature, humidity
             FROM sensor_data
             ORDER BY node_name, time_received"
        );

        respond(200, $result->fetch_all(MYSQLI_ASSOC));
    }

    // GET with normal or Base64 parameters
    if ($method === 'GET') {
        if (isset($_GET['nodeId']) || isset($_GET['node_name'])) {
            $input = $_GET;
        } else {
            $isBase64 = true;

            $encoded = $_SERVER['QUERY_STRING'];
            $decoded = base64_decode($encoded, true);

            if ($decoded === false) {
                respond(400, ["error" => "Invalid Base64"]);
            }

            parse_str($decoded, $input);
        }
    }

    // POST: Insert new reading
    elseif ($method === 'POST') {
        $input = $_POST;
    }

    // PUT: Update latest reading
    elseif ($method === 'PUT') {
        parse_str(file_get_contents("php://input"), $input);
    }

    else {
        header("Allow: GET, POST, PUT");
        respond(405, ["error" => "Method not allowed"]);
    }

    // Extract values
    $node = $input['nodeId'] ?? $input['node_name'] ?? '';
    $temp = $input['nodeTemp'] ?? $input['temperature'] ?? '';
    $humidity = $input['humidity'] ?? '';
    $time = $input['timeReceived'] ?? $input['time_received'] ?? '';

    // Validate sensor values
    if (!is_string($node) ||
        !preg_match('/^node_[1-9][0-9]*$/', $node) ||
        strlen($node) > 10 ||
        !is_scalar($temp) || !is_numeric($temp) ||
        !is_scalar($humidity) || !is_numeric($humidity) ||
        !is_string($time) ||
        $temp < -10 || $temp > 100 ||
        $humidity < 0 || $humidity > 100) {
        respond(400, ["error" => "Invalid sensor data"]);
    }

    // Verify registered node
    $check = $conn->prepare(
        "SELECT node_name FROM sensor_register WHERE node_name = ?"
    );

    $check->bind_param("s", $node);
    $check->execute();

    if ($check->get_result()->num_rows === 0) {
        respond(400, ["error" => "Node is not registered"]);
    }

    $check->close();

    // PUT: Update latest reading
    if ($method === 'PUT') {
        $stmt = $conn->prepare(
            "UPDATE sensor_data
             SET temperature = ?, humidity = ?
             WHERE node_name = ?
             ORDER BY time_received DESC
             LIMIT 1"
        );

        $stmt->bind_param("dds", $temp, $humidity, $node);
        $stmt->execute();

        respond(200, [
            "message" => $stmt->affected_rows > 0
                ? "Latest reading updated successfully"
                : "No rows changed or node has no readings"
        ]);
    }

    // GET or POST: Insert reading
    $formattedTime = formatTime($time);

    if ($formattedTime === false) {
        respond(400, ["error" => "Invalid timestamp"]);
    }

    if ($formattedTime === null) {
        // Database automatically generates timestamp
        $stmt = $conn->prepare(
            "INSERT INTO sensor_data
             (node_name, temperature, humidity)
             VALUES (?, ?, ?)"
        );

        $stmt->bind_param("sdd", $node, $temp, $humidity);
    } else {
        // Use timestamp supplied in URL
        $stmt = $conn->prepare(
            "INSERT INTO sensor_data
             (node_name, time_received, temperature, humidity)
             VALUES (?, ?, ?, ?)"
        );

        $stmt->bind_param(
            "ssdd", $node, $formattedTime, $temp, $humidity
        );
    }

    $stmt->execute();
    $stmt->close();

    // Display inserted values in browser
    respond(200, [
        "message" => $isBase64
            ? "Base64 data inserted successfully"
            : "Data inserted successfully",
        "node_name" => $node,
        "time_received" => $formattedTime ?? "Automatically generated",
        "temperature" => (float)$temp,
        "humidity" => (float)$humidity
    ]);

} catch (mysqli_sql_exception $e) {
    error_log($e->getMessage());
    respond(400, ["error" => "Database operation failed"]);
}
?>
