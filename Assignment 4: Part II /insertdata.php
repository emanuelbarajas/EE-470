
<?php
$host = "localhost";
$db = "u230390396_EmanuelBarajas";
$user = "u230390396_db_EmanuelBara";
$pass = "@Redsonguy1";

header("Content-Type: application/json");

try {
    $conn = new mysqli($host, $user, $pass, $db);
    $conn->set_charset("utf8mb4");

    $method = $_SERVER['REQUEST_METHOD'];

    // GET with Base64: Decode and insert sensor data
    if ($method == 'GET' && !empty($_SERVER['QUERY_STRING'])) {

        $encoded = $_SERVER['QUERY_STRING'];
        $decoded = base64_decode($encoded, true);

        if ($decoded === false) {
            http_response_code(400);
            exit(json_encode(["error" => "Invalid Base64"]));
        }

        parse_str($decoded, $input);

        $node = $input['nodeId'] ?? '';
        $temp = $input['nodeTemp'] ?? '';
        $humidity = $input['humidity'] ?? '';
        $time = $input['timeReceived'] ?? '';

        if (!preg_match('/^node_[1-5]$/', $node) ||
            !is_numeric($temp) ||
            !is_numeric($humidity) ||
            $temp < -10 || $temp > 100 ||
            $humidity < 0 || $humidity > 100) {
            http_response_code(400);
            exit(json_encode(["error" => "Invalid sensor data"]));
        }

        if ($time === '') {
            $stmt = $conn->prepare(
                "INSERT INTO sensor_data
                 (node_name, temperature, humidity)
                 VALUES (?, ?, ?)"
            );

            $stmt->bind_param("sdd", $node, $temp, $humidity);
        } else {
            $date = DateTimeImmutable::createFromFormat(
                '!Y-m-d H:i:s', $time
            );

            if (!$date || $date->format('Y-m-d H:i:s') !== $time) {
                http_response_code(400);
                exit(json_encode(["error" => "Invalid timestamp"]));
            }

            $stmt = $conn->prepare(
                "INSERT INTO sensor_data
                 (node_name, time_received, temperature, humidity)
                 VALUES (?, ?, ?, ?)"
            );

            $stmt->bind_param("ssdd", $node, $time, $temp, $humidity);
        }

        $stmt->execute();

        echo json_encode([
            "message" => "Base64 data inserted successfully",
            "node_name" => $node,
            "temperature" => (float)$temp,
            "humidity" => (float)$humidity
        ]);

        $stmt->close();
    }

    // GET without Base64: Retrieve all sensor data
    elseif ($method == 'GET') {

        $result = $conn->query(
            "SELECT node_name, time_received, temperature, humidity
             FROM sensor_data
             ORDER BY node_name, time_received"
        );

        echo json_encode($result->fetch_all(MYSQLI_ASSOC));
    }

    // POST and PUT: Receive sensor data
    elseif ($method == 'POST' || $method == 'PUT') {

        if ($method == 'POST') {
            $input = $_POST;
        } else {
            parse_str(file_get_contents("php://input"), $input);
        }

        $node = $input['node_name'] ?? '';
        $temp = $input['temperature'] ?? '';
        $humidity = $input['humidity'] ?? '';

        if (!preg_match('/^node_[1-5]$/', $node) ||
            !is_numeric($temp) ||
            !is_numeric($humidity) ||
            $temp < -10 || $temp > 100 ||
            $humidity < 0 || $humidity > 100) {
            http_response_code(400);
            exit(json_encode(["error" => "Invalid data"]));
        }

        // POST: Insert new reading
        if ($method == 'POST') {

            $stmt = $conn->prepare(
                "INSERT INTO sensor_data
                 (node_name, temperature, humidity)
                 VALUES (?, ?, ?)"
            );

            $stmt->bind_param("sdd", $node, $temp, $humidity);
            $stmt->execute();

            echo json_encode([
                "message" => "Data inserted successfully"
            ]);
        }

        // PUT: Update latest reading for selected node
        else {

            $stmt = $conn->prepare(
                "UPDATE sensor_data
                 SET temperature = ?, humidity = ?
                 WHERE node_name = ?
                 ORDER BY time_received DESC
                 LIMIT 1"
            );

            $stmt->bind_param("dds", $temp, $humidity, $node);
            $stmt->execute();

            echo json_encode([
                "message" => $stmt->affected_rows > 0
                    ? "Latest reading updated successfully"
                    : "No rows changed or node has no readings"
            ]);
        }

        $stmt->close();
    }

    // Unsupported HTTP methods
    else {
        http_response_code(405);
        header("Allow: GET, POST, PUT");

        echo json_encode([
            "error" => "Method not allowed"
        ]);
    }

    $conn->close();

} catch (mysqli_sql_exception $e) {
    error_log($e->getMessage());
    http_response_code(400);

    echo json_encode([
        "error" => "Database operation failed"
    ]);
}
?>
