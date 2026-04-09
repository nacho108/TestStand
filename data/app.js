window.addEventListener("load", () => {
  const ui = {
    viewTitle: document.getElementById("view-title"),
    startMotorTestButton: document.getElementById("start-motor-test-button"),
    stopMotorTestButton: document.getElementById("stop-motor-test-button"),
    downloadTestButton: document.getElementById("download-test-button"),
    overviewThrottle: document.getElementById("overview-throttle-value"),
    overviewThrottleHealth: document.getElementById("overview-throttle-health"),
    testingThrottle: document.getElementById("testing-throttle-value"),
    testingThrottleHealth: document.getElementById("testing-throttle-health"),
    voltage: document.getElementById("voltage-value"),
    current: document.getElementById("current-value"),
    power: document.getElementById("power-value"),
    rpm: document.getElementById("rpm-value"),
    escTemp: document.getElementById("esc-temp-value"),
    motorTemp: document.getElementById("ir-object-value"),
    ambientTemp: document.getElementById("ir-ambient-value"),
    thrust: document.getElementById("thrust-value"),
    thrustStdDev: document.getElementById("thrust-stddev-value"),
    voltageHealth: document.getElementById("voltage-health"),
    currentHealth: document.getElementById("current-health"),
    powerHealth: document.getElementById("power-health"),
    rpmHealth: document.getElementById("rpm-health"),
    thrustHealth: document.getElementById("thrust-health"),
    escTempHealth: document.getElementById("esc-temp-health"),
    motorTempHealth: document.getElementById("motor-temp-health"),
    ambientTempHealth: document.getElementById("ambient-temp-health"),
    chartThrustPath: document.querySelector(".chart-line--thrust path"),
    chartPowerPath: document.querySelector(".chart-line--power path"),
    chartRpmPath: document.querySelector(".chart-line--rpm path"),
    chartTempPath: document.querySelector(".chart-line--temp path"),
    chartLeftTopPower: document.getElementById("chart-left-top-power"),
    chartLeftMidPower: document.getElementById("chart-left-mid-power"),
    chartLeftBottomPower: document.getElementById("chart-left-bottom-power"),
    chartLeftTopRpm: document.getElementById("chart-left-top-rpm"),
    chartLeftMidRpm: document.getElementById("chart-left-mid-rpm"),
    chartLeftBottomRpm: document.getElementById("chart-left-bottom-rpm"),
    chartRightTopThrust: document.getElementById("chart-right-top-thrust"),
    chartRightMidThrust: document.getElementById("chart-right-mid-thrust"),
    chartRightBottomThrust: document.getElementById("chart-right-bottom-thrust"),
    chartRightTopTemp: document.getElementById("chart-right-top-temp"),
    chartRightMidTemp: document.getElementById("chart-right-mid-temp"),
    chartRightBottomTemp: document.getElementById("chart-right-bottom-temp")
  };

  const links = Array.from(document.querySelectorAll("[data-view-link]"));
  const panes = Array.from(document.querySelectorAll("[data-view-pane]"));

  const setText = (element, value) => {
    if (element) {
      element.textContent = value;
    }
  };

  const setHealth = (element, text, state = "ok") => {
    if (!element) {
      return;
    }

    element.classList.remove("metric-row__health--warn", "metric-row__health--error");
    if (state === "warn") {
      element.classList.add("metric-row__health--warn");
    } else if (state === "error") {
      element.classList.add("metric-row__health--error");
    }
    element.textContent = text;
  };

  const setView = (view) => {
    links.forEach((link) => {
      link.classList.toggle("sidebar__nav-link--active", link.dataset.viewLink === view);
    });

    panes.forEach((pane) => {
      pane.hidden = pane.dataset.viewPane !== view;
    });

    if (ui.viewTitle) {
      ui.viewTitle.textContent = view === "testing" ? "Testing" : "Live overview";
    }
  };

  links.forEach((link) => {
    link.addEventListener("click", (event) => {
      event.preventDefault();
      const { viewLink } = link.dataset;
      if (viewLink) {
        setView(viewLink);
      }
    });
  });

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

  const applyDisconnectedState = () => {
    setHealth(ui.overviewThrottleHealth, "Waiting for motor command", "warn");
    setHealth(ui.testingThrottleHealth, "Waiting for motor command", "warn");
    setHealth(ui.voltageHealth, "Waiting for ESC telemetry", "error");
    setHealth(ui.currentHealth, "Waiting for ESC telemetry", "error");
    setHealth(ui.powerHealth, "Waiting for live feed", "error");
    setHealth(ui.rpmHealth, "Waiting for telemetry", "error");
    setHealth(ui.escTempHealth, "Waiting for ESC telemetry", "error");
    setHealth(ui.thrustHealth, "Waiting for scale", "warn");
    setHealth(ui.motorTempHealth, "Waiting for IR sensor", "warn");
    setHealth(ui.ambientTempHealth, "Waiting for IR sensor", "warn");
    if (ui.stopMotorTestButton) {
      ui.stopMotorTestButton.disabled = true;
    }
    if (ui.downloadTestButton) {
      ui.downloadTestButton.disabled = true;
    }
  };

  const formatScaleValue = (value, unit, digits = 0) => {
    if (!Number.isFinite(value)) {
      return `-- ${unit}`;
    }

    return `${Number(value).toFixed(digits)} ${unit}`;
  };

  const setScaleLabels = (topElement, midElement, bottomElement, maxValue, unit, digits = 0) => {
    setText(topElement, formatScaleValue(maxValue, unit, digits));
    setText(midElement, formatScaleValue(maxValue / 2, unit, digits));
    setText(bottomElement, formatScaleValue(0, unit, digits));
  };

  const getSeriesMax = (rows, valueKey) => {
    if (!Array.isArray(rows) || rows.length === 0) {
      return 0;
    }

    return rows.reduce((max, row) => {
      const value = Number(row?.[valueKey]);
      if (!Number.isFinite(value)) {
        return max;
      }

      return Math.max(max, value);
    }, 0);
  };

  const updateChartScales = (rows) => {
    const maxPower = getSeriesMax(rows, "power_w");
    const maxRpm = getSeriesMax(rows, "rpm");
    const maxThrust = getSeriesMax(rows, "thrust_grams");
    const maxTemp = getSeriesMax(rows, "motor_temperature_c");

    setScaleLabels(ui.chartLeftTopPower, ui.chartLeftMidPower, ui.chartLeftBottomPower, maxPower, "W", 0);
    setScaleLabels(ui.chartLeftTopRpm, ui.chartLeftMidRpm, ui.chartLeftBottomRpm, maxRpm, "rpm", 0);
    setScaleLabels(ui.chartRightTopThrust, ui.chartRightMidThrust, ui.chartRightBottomThrust, maxThrust, "g", 0);
    setScaleLabels(ui.chartRightTopTemp, ui.chartRightMidTemp, ui.chartRightBottomTemp, maxTemp, "C", 1);
  };

  const buildChartPath = (rows, valueKey) => {
    if (!Array.isArray(rows) || rows.length === 0) {
      return "M0,100 L100,100";
    }

    const points = rows
      .map((row) => {
        const x = Number(row?.throttle_percent);
        const y = Number(row?.[valueKey]);
        if (Number.isNaN(x) || Number.isNaN(y)) {
          return null;
        }

        return { x, y };
      })
      .filter(Boolean);

    if (points.length === 0) {
      return "M0,100 L100,100";
    }

    const maxY = points.reduce((max, point) => Math.max(max, point.y), 0);
    const safeMaxY = maxY > 0 ? maxY : 1;

    return points
      .map((point, index) => {
        const px = Math.max(0, Math.min(100, point.x));
        const py = 100 - (point.y / safeMaxY) * 100;
        const clampedY = Math.max(0, Math.min(100, py));
        return `${index === 0 ? "M" : "L"}${px.toFixed(2)},${clampedY.toFixed(2)}`;
      })
      .join(" ");
  };

  const updateTestChart = (rows) => {
    updateChartScales(rows);

    if (ui.chartThrustPath) {
      ui.chartThrustPath.setAttribute("d", buildChartPath(rows, "thrust_grams"));
    }

    if (ui.chartPowerPath) {
      ui.chartPowerPath.setAttribute("d", buildChartPath(rows, "power_w"));
    }

    if (ui.chartRpmPath) {
      ui.chartRpmPath.setAttribute("d", buildChartPath(rows, "rpm"));
    }

    if (ui.chartTempPath) {
      ui.chartTempPath.setAttribute("d", buildChartPath(rows, "motor_temperature_c"));
    }
  };

  const applyStatus = (data) => {
    const hasTelemetry = Boolean(data.telemetry_valid);
    const hasIr = Boolean(data.ir_detected);
    const hasScale = Boolean(data.scale_detected);
    const scaleReady = hasScale && Boolean(data.scale_valid);
    const resultCount = Number(data.test_result_count) || 0;
    const hasInlineResults = Array.isArray(data.test_results);

    if (hasInlineResults) {
      cachedTestResults = data.test_results;
      cachedTestResultCount = resultCount;
    }

    setText(ui.voltage, formatNumber(data.voltage_v, "V", 3));
    setText(ui.current, formatNumber(data.current_a, "A", 3));
    setText(ui.power, formatNumber(data.power_w, "W", 2));
    setText(ui.overviewThrottle, formatNumber(data.throttle_percent, "%", 1));
    setText(ui.testingThrottle, formatNumber(data.throttle_percent, "%", 1));
    setText(ui.rpm, formatInteger(data.rpm, "rpm"));
    setText(ui.escTemp, formatNumber(data.esc_temperature_c, "deg C", 1));
    setText(ui.motorTemp, formatNumber(data.ir_object_c, "deg C", 1));
    setText(ui.ambientTemp, formatNumber(data.ir_ambient_c, "deg C", 1));
    setText(ui.thrust, formatNumber(data.thrust_grams, "g", 1));
    setText(ui.thrustStdDev, `Std dev ${formatNumber(data.thrust_stddev_grams, "g", 2)}`);
    updateTestChart(cachedTestResults);

    if (Boolean(data.test_running)) {
      lastKnownTestRunning = true;
      motorTestPending = true;
      setMotorTestButtonState("Test running...", true);
      setStopMotorTestButtonState(false);
      setDownloadTestButtonState(true);
    } else {
      lastKnownTestRunning = false;
      motorTestPending = false;
      setMotorTestButtonState("Start test", false);
      setStopMotorTestButtonState(true);
      setDownloadTestButtonState(resultCount === 0);
    }

    if (hasTelemetry) {
      setHealth(ui.overviewThrottleHealth, "Throttle command active");
      setHealth(ui.testingThrottleHealth, "Throttle command active");
      setHealth(ui.voltageHealth, "ESC telemetry healthy");
      setHealth(ui.currentHealth, "Current sensor healthy");
      setHealth(ui.powerHealth, "Computed from live feed");
      setHealth(ui.rpmHealth, "RPM telemetry healthy");
      setHealth(ui.escTempHealth, "ESC thermal sensor healthy");
    } else {
      setHealth(ui.overviewThrottleHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.testingThrottleHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.voltageHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.currentHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.powerHealth, "Waiting for live feed", "warn");
      setHealth(ui.rpmHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.escTempHealth, "Awaiting ESC telemetry", "warn");
    }

    if (scaleReady) {
      setHealth(ui.thrustHealth, "Load cell healthy");
    } else if (hasScale) {
      setHealth(ui.thrustHealth, "Scale stabilizing", "warn");
    } else {
      setHealth(ui.thrustHealth, "Scale offline", "error");
    }

    if (hasIr) {
      setHealth(ui.motorTempHealth, "IR target healthy");
      setHealth(ui.ambientTempHealth, "Ambient sensor healthy");
    } else {
      setHealth(ui.motorTempHealth, "IR sensor offline", "error");
      setHealth(ui.ambientTempHealth, "IR sensor offline", "error");
    }

    return {
      resultCount,
      hasInlineResults
    };
  };

  let socket = null;
  let reconnectTimer = null;
  let pollTimer = null;
  let motorTestPending = false;
  let lastKnownTestRunning = false;
  let cachedTestResults = [];
  let cachedTestResultCount = 0;

  setView("overview");
  applyDisconnectedState();

  const stopPolling = () => {
    if (pollTimer !== null) {
      window.clearInterval(pollTimer);
      pollTimer = null;
    }
  };

  const pollStatus = async () => {
    try {
      const response = await fetch("/api/status", { cache: "no-store" });
      if (!response.ok) {
        throw new Error("HTTP status not ok");
      }

      applyStatus(await response.json());
      setHealth(ui.powerHealth, "HTTP polling active", "warn");
    } catch (error) {
      applyDisconnectedState();
      setHealth(ui.powerHealth, "Waiting for live feed", "error");
    }
  };

  const startPolling = () => {
    if (pollTimer !== null) {
      return;
    }

    pollStatus();
    pollTimer = window.setInterval(pollStatus, 1000);
  };

  const setMotorTestButtonState = (label, disabled = false) => {
    if (ui.startMotorTestButton) {
      ui.startMotorTestButton.textContent = label;
      ui.startMotorTestButton.disabled = disabled;
    }
  };

  const setStopMotorTestButtonState = (disabled = true) => {
    if (ui.stopMotorTestButton) {
      ui.stopMotorTestButton.disabled = disabled;
    }
  };

  const setDownloadTestButtonState = (disabled = true) => {
    if (ui.downloadTestButton) {
      ui.downloadTestButton.disabled = disabled;
    }
  };

  const runMotorTest = async () => {
    if (!ui.startMotorTestButton || motorTestPending) {
      return;
    }

    motorTestPending = true;
    setMotorTestButtonState("Starting...", true);
    setStopMotorTestButtonState(true);
    setDownloadTestButtonState(true);
    cachedTestResults = [];
    cachedTestResultCount = 0;
    updateTestChart([]);

    try {
      const body = new URLSearchParams({ cmd: "motor test" });
      const response = await fetch("/api/command", {
        method: "POST",
        headers: {
          "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8"
        },
        body
      });

      if (!response.ok) {
        throw new Error("HTTP status not ok");
      }

      setMotorTestButtonState("Test running...", true);
      setStopMotorTestButtonState(false);
    } catch (error) {
      motorTestPending = false;
      setMotorTestButtonState("Start test", false);
      setStopMotorTestButtonState(true);
      setHealth(ui.powerHealth, "Failed to start motor test", "error");
    }
  };

  const stopMotorTest = async () => {
    if (!ui.stopMotorTestButton || ui.stopMotorTestButton.disabled) {
      return;
    }

    setStopMotorTestButtonState(true);
    setHealth(ui.powerHealth, "Stopping motor test...", "warn");

    try {
      const body = new URLSearchParams({ cmd: "motor test stop" });
      const response = await fetch("/api/command", {
        method: "POST",
        headers: {
          "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8"
        },
        body
      });

      if (!response.ok) {
        throw new Error("HTTP status not ok");
      }
    } catch (error) {
      if (lastKnownTestRunning) {
        setStopMotorTestButtonState(false);
      }
      setHealth(ui.powerHealth, "Failed to stop motor test", "error");
    }
  };

  const downloadTestFile = () => {
    if (!ui.downloadTestButton || ui.downloadTestButton.disabled) {
      return;
    }

    const link = document.createElement("a");
    link.href = "/api/test.csv";
    link.download = "test_results.csv";
    document.body.appendChild(link);
    link.click();
    link.remove();
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
      stopPolling();
      applyDisconnectedState();
      setHealth(ui.powerHealth, "WebSocket connected");
    });

    socket.addEventListener("message", (event) => {
      try {
        const data = JSON.parse(event.data);
        const statusInfo = applyStatus(data);
        if (statusInfo.resultCount !== cachedTestResultCount ||
            (!statusInfo.hasInlineResults && statusInfo.resultCount > 0 && cachedTestResults.length === 0)) {
          cachedTestResultCount = statusInfo.resultCount;
          pollStatus();
        }
        if (!data.test_running) {
          lastKnownTestRunning = false;
          motorTestPending = false;
          setMotorTestButtonState("Start test", false);
          setStopMotorTestButtonState(true);
          setDownloadTestButtonState((Number(data.test_result_count) || 0) === 0);
        } else {
          lastKnownTestRunning = true;
          setMotorTestButtonState("Test running...", true);
          setStopMotorTestButtonState(false);
          setDownloadTestButtonState(true);
        }
      } catch (error) {
        setHealth(ui.powerHealth, "Malformed live payload", "error");
      }
    });

    socket.addEventListener("close", () => {
      applyDisconnectedState();
      startPolling();
      scheduleReconnect();
    });

    socket.addEventListener("error", () => {
      startPolling();
      if (socket) {
        socket.close();
      }
    });
  };

  startPolling();
  connectWebSocket();

  if (ui.startMotorTestButton) {
    ui.startMotorTestButton.addEventListener("click", runMotorTest);
  }

  if (ui.stopMotorTestButton) {
    ui.stopMotorTestButton.addEventListener("click", stopMotorTest);
  }

  if (ui.downloadTestButton) {
    ui.downloadTestButton.addEventListener("click", downloadTestFile);
  }
});
