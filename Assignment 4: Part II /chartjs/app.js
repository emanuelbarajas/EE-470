
$(document).ready(function() {

    // Change "bar" to "line" to switch graph types
    var chartType = "bar";

    var sensorData = [];
    var barGraph = null;

    // Retrieve sensor readings using AJAX GET
    $.ajax({
        url: "insertdata.php",
        method: "GET",
        dataType: "json",
        cache: false,

        success: function(data) {
            console.log("Received sensor data:", data);

            // Verify PHP returned a JSON array
            if (!Array.isArray(data)) {
                console.error("Expected JSON array:", data);
                $("#stats").text("Invalid sensor data response");
                return;
            }

            sensorData = data;
            updateNode();
        },

        error: function(xhr, status, error) {
            console.error("AJAX error:", status, error);
            console.error("HTTP status:", xhr.status);
            console.error("Server response:", xhr.responseText);

            $("#stats").text(
                "Error loading sensor data (HTTP " + xhr.status + ")"
            );
        }
    });

    // Update when selected node changes
    $("#nodeSelect").change(function() {
        updateNode();
    });

    function updateNode() {

        var node = $("#nodeSelect").val();

        var readings = sensorData.filter(function(row) {
            return row.node_name === node;
        });

        var x_axis = [];
        var y_axis = [];
        var humidity = [];

        readings.forEach(function(row) {
            x_axis.push(row.time_received);
            y_axis.push(Number(row.temperature));
            humidity.push(Number(row.humidity));
        });

        if (readings.length === 0) {
            $("#stats").text("No readings available for " + node);

            if (barGraph) {
                barGraph.destroy();
                barGraph = null;
            }

            return;
        }

        // Calculate statistics
        var avgTemp =
            y_axis.reduce((a, b) => a + b, 0) / y_axis.length;

        var avgHumidity =
            humidity.reduce((a, b) => a + b, 0) / humidity.length;

        var maxTemp = Math.max(...y_axis);
        var minHumidity = Math.min(...humidity);

        // Display statistics
        $("#stats").html(
            "Average Temperature for " + node + ": " +
            avgTemp.toFixed(2) + "°C<br>" +

            "Average Humidity for " + node + ": " +
            avgHumidity.toFixed(2) + "%<br>" +

            "Maximum Temperature for " + node + ": " +
            maxTemp.toFixed(2) + "°C<br>" +

            "Minimum Humidity for " + node + ": " +
            minHumidity.toFixed(2) + "%"
        );

        // Remove previous graph
        if (barGraph) {
            barGraph.destroy();
        }

        var chartdata = {
            labels: x_axis,

            datasets: [{
                label: node + " Temperature",
                backgroundColor: "rgba(0, 255, 0, 0.70)",
                borderColor: "rgba(0, 180, 0, 1)",
                borderWidth: 2,
                data: y_axis
            }]
        };

        // Create Chart.js graph
        barGraph = new Chart(
            document.getElementById("mycanvas"),
            {
                type: chartType,
                data: chartdata,

                options: {
                    responsive: true,

                    scales: {
                        x: {
                            title: {
                                display: true,
                                text: "Time Received"
                            }
                        },

                        y: {
                            title: {
                                display: true,
                                text: "Temperature (°C)"
                            },
                            beginAtZero: true
                        }
                    }
                }
            }
        );
    }
});
