window.addEventListener("load", () => {
  const body = document.body;
  const ui = {
    hostingStatus: document.getElementById("hosting-status"),
    apiStatus: document.getElementById("api-status"),
    lastRefresh: document.getElementById("last-refresh"),
    telemetryHealth: document.getElementById("telemetry-health"),
    telemetrySummary: document.getElementById("telemetry-summary"),
    refreshState: document.getElementById("refresh-state"),
    timestampValue: document.getElementById("timestamp-value"),
    voltage: document.getElementById("voltage-value"),
    current: document.getElementById("current-value"),
    power: document.getElementById("power-value"),
    rpm: document.getElementById("rpm-value"),
    escTemp: document.getElementById("esc-temp-value"),
    irAmbient: document.getElementById("ir-ambient-value"),
    irObject: document.getElementById("ir-object-value"),
    thrust: document.getElementById("thrust-value")
  };

  const setText = (element, value) => {
    if (element) {
      element.textContent = value;
    }
  };

  const formatNumber = (value, unit, digits = 2) => {
    if (value === null || value === undefined || Number.isNaN(Number(value))) {
      return "--";
    }

    return `${Number(value).toFixed(digits)} ${unit}`;
  };

  const formatInteger = (value, unit) => {
    if (value === null || value === undefined || Number.isNaN(Number(value))) {
      return "--";
    }

    return `${Math.round(Number(value))} ${unit}`;
  };

  const updateHealth = (isOk, summary) => {
    body.classList.toggle("status-ok", isOk);
    body.classList.toggle("status-error", !isOk);
    setText(ui.telemetrySummary, summary);
  };

  let socket = null;
  let reconnectTimer = null;

  setText(ui.hostingStatus, "Dashboard ready");
  setText(ui.refreshState, "Connecting to live updates");
  updateHealth(false, "Metrics will update automatically once the WebSocket connects.");

  const applyStatus = (data) => {
    const now = new Date();
    const hasTelemetry = Boolean(data.telemetry_valid);
    const sensorSummary = [
      data.ir_detected ? "IR ready" : "IR offline",
      data.scale_detected ? (data.scale_valid ? "scale ready" : "scale waiting") : "scale offline"
    ].join(", ");

    setText(ui.apiStatus, hasTelemetry ? "Live status OK" : "Connected, waiting");
    setText(ui.lastRefresh, now.toLocaleTimeString());
    setText(ui.telemetryHealth, hasTelemetry ? "Telemetry streaming" : "Awaiting telemetry");
    setText(
      ui.refreshState,
      hasTelemetry ? "WebSocket live at 4 Hz" : "WebSocket connected, no ESC frame yet"
    );
    setText(
      ui.timestampValue,
      `Device uptime ${formatInteger(data.timestamp_ms / 1000, "s")}`
    );

    updateHealth(
      true,
      hasTelemetry
        ? `Telemetry is live with ${sensorSummary}.`
        : `WebSocket is connected. ${sensorSummary}.`
    );

    setText(ui.voltage, formatNumber(data.voltage_v, "V", 3));
    setText(ui.current, formatNumber(data.current_a, "A", 3));
    setText(ui.power, formatNumber(data.power_w, "W", 2));
    setText(ui.rpm, formatInteger(data.rpm, "rpm"));
    setText(ui.escTemp, formatNumber(data.esc_temperature_c, "deg C", 1));
    setText(ui.irAmbient, formatNumber(data.ir_ambient_c, "deg C", 1));
    setText(ui.irObject, formatNumber(data.ir_object_c, "deg C", 1));
    setText(ui.thrust, formatNumber(data.thrust_grams, "g", 1));
  };

  const scheduleReconnect = () => {
    if (reconnectTimer !== null) {
      return;
    }

    reconnectTimer = window.setTimeout(() => {
      reconnectTimer = null;
      connectWebSocket();
    }, 1500);
  };

  const connectWebSocket = () => {
    if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
      return;
    }

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    socket = new WebSocket(`${protocol}//${window.location.host}/ws`);

    socket.addEventListener("open", () => {
      setText(ui.apiStatus, "WebSocket connected");
      setText(ui.refreshState, "Waiting for live data");
      setText(ui.lastRefresh, "Connected");
      updateHealth(false, "WebSocket connected. Waiting for the first telemetry frame.");
    });

    socket.addEventListener("message", (event) => {
      try {
        applyStatus(JSON.parse(event.data));
      } catch (error) {
        setText(ui.apiStatus, "Invalid update");
        setText(ui.refreshState, "Malformed WebSocket payload");
        updateHealth(false, "The device sent a WebSocket message that could not be parsed.");
      }
    });

    socket.addEventListener("close", () => {
      setText(ui.apiStatus, "Disconnected");
      setText(ui.lastRefresh, "Reconnecting...");
      setText(ui.telemetryHealth, "Connection issue");
      setText(ui.refreshState, "Retrying WebSocket");
      setText(ui.timestampValue, "Waiting for device timestamp");
      updateHealth(false, "The live update channel disconnected. Check the ESP32 AP and web server.");
      scheduleReconnect();
    });

    socket.addEventListener("error", () => {
      if (socket) {
        socket.close();
      }
    });
  };

  connectWebSocket();
});
