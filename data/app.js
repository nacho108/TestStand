window.addEventListener("load", () => {
  const ui = {
    viewTitle: document.getElementById("view-title"),
    startMotorTestButton: document.getElementById("start-motor-test-button"),
    stopMotorTestButton: document.getElementById("stop-motor-test-button"),
    downloadTestButton: document.getElementById("download-test-button"),
    studyFileSelect: document.getElementById("study-file-select"),
    loadStudyButton: document.getElementById("load-study-button"),
    openStudyFileButton: document.getElementById("open-study-file-button"),
    clearStudyButton: document.getElementById("clear-study-button"),
    studyLocalFileInput: document.getElementById("study-local-file-input"),
    studyStatus: document.getElementById("study-status"),
    studyXAxisInputs: Array.from(document.querySelectorAll('input[name="study-x-axis"]')),
    studySeriesInputs: Array.from(document.querySelectorAll('input[name="study-series"]')),
    studyThrustSeriesOption: document.getElementById("study-series-thrust")?.closest("label") || null,
    studyFileVisibilityEmpty: document.getElementById("study-file-visibility-empty"),
    studyFileVisibilityList: document.getElementById("study-file-visibility-list"),
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
    ambientTempHealth: document.getElementById("ambient-temp-health")
  };

  const chartContexts = {
    testing: {
      thrustPath: document.getElementById("testing-chart-thrust-path"),
      powerPath: document.getElementById("testing-chart-power-path"),
      rpmPath: document.getElementById("testing-chart-rpm-path"),
      tempPath: document.getElementById("testing-chart-temp-path"),
      leftTopPower: document.getElementById("testing-chart-left-top-power"),
      leftMidPower: document.getElementById("testing-chart-left-mid-power"),
      leftBottomPower: document.getElementById("testing-chart-left-bottom-power"),
      leftTopRpm: document.getElementById("testing-chart-left-top-rpm"),
      leftMidRpm: document.getElementById("testing-chart-left-mid-rpm"),
      leftBottomRpm: document.getElementById("testing-chart-left-bottom-rpm"),
      rightTopThrust: document.getElementById("testing-chart-right-top-thrust"),
      rightMidThrust: document.getElementById("testing-chart-right-mid-thrust"),
      rightBottomThrust: document.getElementById("testing-chart-right-bottom-thrust"),
      rightTopTemp: document.getElementById("testing-chart-right-top-temp"),
      rightMidTemp: document.getElementById("testing-chart-right-mid-temp"),
      rightBottomTemp: document.getElementById("testing-chart-right-bottom-temp")
    },
    study: {
      thrustLayer: document.getElementById("study-chart-thrust-layer"),
      powerLayer: document.getElementById("study-chart-power-layer"),
      rpmLayer: document.getElementById("study-chart-rpm-layer"),
      tempLayer: document.getElementById("study-chart-temp-layer"),
      leftTopPower: document.getElementById("study-chart-left-top-power"),
      leftMidPower: document.getElementById("study-chart-left-mid-power"),
      leftBottomPower: document.getElementById("study-chart-left-bottom-power"),
      leftTopRpm: document.getElementById("study-chart-left-top-rpm"),
      leftMidRpm: document.getElementById("study-chart-left-mid-rpm"),
      leftBottomRpm: document.getElementById("study-chart-left-bottom-rpm"),
      rightTopThrust: document.getElementById("study-chart-right-top-thrust"),
      rightMidThrust: document.getElementById("study-chart-right-mid-thrust"),
      rightBottomThrust: document.getElementById("study-chart-right-bottom-thrust"),
      rightTopTemp: document.getElementById("study-chart-right-top-temp"),
      rightMidTemp: document.getElementById("study-chart-right-mid-temp"),
      rightBottomTemp: document.getElementById("study-chart-right-bottom-temp"),
      xLabel: document.querySelector('[data-view-pane="study"] .chart-plot__xlabel'),
      xTicks: [
        document.getElementById("study-chart-tick-0"),
        document.getElementById("study-chart-tick-1"),
        document.getElementById("study-chart-tick-2"),
        document.getElementById("study-chart-tick-3"),
        document.getElementById("study-chart-tick-4")
      ],
      xAxisKey: "thrust_grams",
      visibleSeries: {
        thrust: true,
        power: true,
        rpm: true,
        temp: true
      }
    }
  };

  const links = Array.from(document.querySelectorAll("[data-view-link]"));
  const panes = Array.from(document.querySelectorAll("[data-view-pane]"));
  const viewTitles = {
    overview: "Live overview",
    testing: "Testing",
    study: "Study"
  };

  let socket = null;
  let reconnectTimer = null;
  let pollTimer = null;
  let motorTestPending = false;
  let lastKnownTestRunning = false;
  let cachedTestResults = [];
  let cachedTestResultCount = 0;
  let savedTestFiles = [];
  let studyDatasets = [];

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

  const formatAxisTick = (value, unit = "", digits = 0) => {
    if (!Number.isFinite(value)) {
      return "--";
    }

    const suffix = unit ? ` ${unit}` : "";
    return `${Number(value).toFixed(digits)}${suffix}`;
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

  const getAxisMax = (rows, valueKey, fallback = 0) => {
    const maxValue = getSeriesMax(rows, valueKey);
    return maxValue > 0 ? maxValue : fallback;
  };

  const flattenStudyRows = (datasets, visibleOnly = false) => {
    if (!Array.isArray(datasets) || datasets.length === 0) {
      return [];
    }

    return datasets.flatMap((dataset) => {
      if (!dataset || !Array.isArray(dataset.rows)) {
        return [];
      }

      if (visibleOnly && dataset.visible === false) {
        return [];
      }

      return dataset.rows;
    });
  };

  const updateChartScales = (context, rows) => {
    const maxPower = getSeriesMax(rows, "power_w");
    const maxRpm = getSeriesMax(rows, "rpm");
    const maxThrust = getSeriesMax(rows, "thrust_grams");
    const maxTemp = getSeriesMax(rows, "motor_temperature_c");

    setScaleLabels(context.leftTopPower, context.leftMidPower, context.leftBottomPower, maxPower, "W", 0);
    setScaleLabels(context.leftTopRpm, context.leftMidRpm, context.leftBottomRpm, maxRpm, "rpm", 0);
    setScaleLabels(context.rightTopThrust, context.rightMidThrust, context.rightBottomThrust, maxThrust, "g", 0);
    setScaleLabels(context.rightTopTemp, context.rightMidTemp, context.rightBottomTemp, maxTemp, "C", 1);
  };

  const updateXAxis = (context, rows) => {
    if (!context.xLabel || !Array.isArray(context.xTicks) || context.xTicks.length === 0) {
      return;
    }

    const isThrustAxis = context.xAxisKey === "thrust_grams";
    const unit = isThrustAxis ? "g" : "";
    const digits = isThrustAxis ? 0 : 0;
    const maxValue = isThrustAxis
      ? getSeriesMax(rows, "thrust_grams")
      : 100;

    setText(context.xLabel, isThrustAxis ? "Thrust" : "Throttle %");
    context.xTicks.forEach((tick, index) => {
      const ratio = context.xTicks.length === 1 ? 0 : index / (context.xTicks.length - 1);
      setText(tick, formatAxisTick(maxValue * ratio, unit, digits));
    });
  };

  const syncStudySeriesControls = () => {
    const thrustOnXAxis = chartContexts.study.xAxisKey === "thrust_grams";

    if (ui.studyThrustSeriesOption) {
      ui.studyThrustSeriesOption.style.display = thrustOnXAxis ? "none" : "";
    }
  };

  const buildStudyDatasetId = (source, name) => `${source}:${name}`;

  const escapeHtml = (text) => String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll("\"", "&quot;")
    .replaceAll("'", "&#39;");

  const buildChartPath = (rows, valueKey, xKey = "throttle_percent", axisMax = {}) => {
    if (!Array.isArray(rows) || rows.length === 0) {
      return "M0,100 L100,100";
    }

    const points = rows
      .map((row) => {
        const x = Number(row?.[xKey]);
        const y = Number(row?.[valueKey]);
        if (!Number.isFinite(x) || !Number.isFinite(y)) {
          return null;
        }

        return { x, y };
      })
      .filter(Boolean);

    if (points.length === 0) {
      return "M0,100 L100,100";
    }

    const hasOriginPoint = points.some((point) => point.x === 0);
    if (!hasOriginPoint) {
      points.push({ x: 0, y: 0 });
    }

    points.sort((a, b) => a.x - b.x);

    const maxX = Number(axisMax.x);
    const maxY = Number(axisMax.y);
    const derivedMaxX = points.reduce((max, point) => Math.max(max, point.x), 0);
    const derivedMaxY = points.reduce((max, point) => Math.max(max, point.y), 0);
    const safeMaxX = Number.isFinite(maxX) && maxX > 0 ? maxX : (derivedMaxX > 0 ? derivedMaxX : 1);
    const safeMaxY = Number.isFinite(maxY) && maxY > 0 ? maxY : (derivedMaxY > 0 ? derivedMaxY : 1);

    return points
      .map((point, index) => {
        const px = (point.x / safeMaxX) * 100;
        const py = 100 - (point.y / safeMaxY) * 100;
        const clampedX = Math.max(0, Math.min(100, px));
        const clampedY = Math.max(0, Math.min(100, py));
        return `${index === 0 ? "M" : "L"}${clampedX.toFixed(2)},${clampedY.toFixed(2)}`;
      })
      .join(" ");
  };

  const getChartAxisBounds = (rows, xAxisKey) => ({
    x: xAxisKey === "thrust_grams"
      ? getAxisMax(rows, "thrust_grams", 1)
      : 100,
    thrust: getAxisMax(rows, "thrust_grams", 1),
    power: getAxisMax(rows, "power_w", 1),
    rpm: getAxisMax(rows, "rpm", 1),
    temp: getAxisMax(rows, "motor_temperature_c", 1)
  });

  const updateChart = (context, rows) => {
    updateChartScales(context, rows);
    updateXAxis(context, rows);
    const xAxisKey = context.xAxisKey || "throttle_percent";
    const visibleSeries = context.visibleSeries || {};
    const hideThrustSeries = xAxisKey === "thrust_grams";
    const axisBounds = getChartAxisBounds(rows, xAxisKey);

    if (context.thrustPath) {
      context.thrustPath.setAttribute("d", buildChartPath(rows, "thrust_grams", xAxisKey, {
        x: axisBounds.x,
        y: axisBounds.thrust
      }));
      context.thrustPath.parentElement.style.display = hideThrustSeries || visibleSeries.thrust === false ? "none" : "";
    }

    if (context.powerPath) {
      context.powerPath.setAttribute("d", buildChartPath(rows, "power_w", xAxisKey, {
        x: axisBounds.x,
        y: axisBounds.power
      }));
      context.powerPath.parentElement.style.display = visibleSeries.power === false ? "none" : "";
    }

    if (context.rpmPath) {
      context.rpmPath.setAttribute("d", buildChartPath(rows, "rpm", xAxisKey, {
        x: axisBounds.x,
        y: axisBounds.rpm
      }));
      context.rpmPath.parentElement.style.display = visibleSeries.rpm === false ? "none" : "";
    }

    if (context.tempPath) {
      context.tempPath.setAttribute("d", buildChartPath(rows, "motor_temperature_c", xAxisKey, {
        x: axisBounds.x,
        y: axisBounds.temp
      }));
      context.tempPath.parentElement.style.display = visibleSeries.temp === false ? "none" : "";
    }
  };

  const buildStudyLayerMarkup = (datasets, valueKey, xKey, className, axisMax) => datasets
    .map((dataset) => {
      const hidden = dataset.visible === false ? " style=\"display:none\"" : "";
      return `<svg class="chart-study-line ${className}" viewBox="0 0 100 100" preserveAspectRatio="none" data-study-dataset-id="${escapeHtml(dataset.id)}"${hidden}><path d="${escapeHtml(buildChartPath(dataset.rows, valueKey, xKey, axisMax))}"></path></svg>`;
    })
    .join("");

  const renderStudyFileControls = () => {
    if (!ui.studyFileVisibilityList || !ui.studyFileVisibilityEmpty) {
      return;
    }

    if (studyDatasets.length === 0) {
      ui.studyFileVisibilityList.innerHTML = "";
      ui.studyFileVisibilityEmpty.hidden = false;
      return;
    }

    ui.studyFileVisibilityEmpty.hidden = true;
    ui.studyFileVisibilityList.innerHTML = studyDatasets
      .map((dataset) => {
        const sourceLabel = dataset.source === "saved" ? "memory" : "PC";
        return `
          <label class="chart-file-picker__option" for="study-dataset-${escapeHtml(dataset.id)}">
            <input id="study-dataset-${escapeHtml(dataset.id)}" type="checkbox" name="study-dataset-visibility" value="${escapeHtml(dataset.id)}"${dataset.visible === false ? "" : " checked"}>
            <span>
              <span class="chart-file-picker__name">${escapeHtml(dataset.name)}</span>
              <span class="chart-file-picker__meta">${escapeHtml(sourceLabel)} | ${dataset.rows.length} pts</span>
            </span>
          </label>
        `;
      })
      .join("");

    ui.studyFileVisibilityList
      .querySelectorAll('input[name="study-dataset-visibility"]')
      .forEach((input) => {
        input.addEventListener("change", () => {
          const dataset = studyDatasets.find((item) => item.id === input.value);
          if (!dataset) {
            return;
          }

          dataset.visible = input.checked;
          updateStudyChart();
        });
      });
  };

  const updateStudyChart = () => {
    const context = chartContexts.study;
    const rows = flattenStudyRows(studyDatasets, true);
    updateChartScales(context, rows);
    updateXAxis(context, rows);

    const xAxisKey = context.xAxisKey || "throttle_percent";
    const visibleSeries = context.visibleSeries || {};
    const hideThrustSeries = xAxisKey === "thrust_grams";
    const axisBounds = getChartAxisBounds(rows, xAxisKey);

    if (context.thrustLayer) {
      context.thrustLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "thrust_grams", xAxisKey, "chart-line--thrust", {
        x: axisBounds.x,
        y: axisBounds.thrust
      });
      context.thrustLayer.style.display = hideThrustSeries || visibleSeries.thrust === false ? "none" : "";
    }

    if (context.powerLayer) {
      context.powerLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "power_w", xAxisKey, "chart-line--power", {
        x: axisBounds.x,
        y: axisBounds.power
      });
      context.powerLayer.style.display = visibleSeries.power === false ? "none" : "";
    }

    if (context.rpmLayer) {
      context.rpmLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "rpm", xAxisKey, "chart-line--rpm", {
        x: axisBounds.x,
        y: axisBounds.rpm
      });
      context.rpmLayer.style.display = visibleSeries.rpm === false ? "none" : "";
    }

    if (context.tempLayer) {
      context.tempLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "motor_temperature_c", xAxisKey, "chart-line--temp", {
        x: axisBounds.x,
        y: axisBounds.temp
      });
      context.tempLayer.style.display = visibleSeries.temp === false ? "none" : "";
    }

    renderStudyFileControls();
  };

  const parseCsvNumber = (value) => {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : null;
  };

  const parseSavedTestCsv = (csv) => {
    if (typeof csv !== "string" || csv.trim().length === 0) {
      return [];
    }

    const lines = csv.trim().split(/\r?\n/);
    if (lines.length < 2) {
      return [];
    }

    return lines.slice(1)
      .map((line) => line.trim())
      .filter((line) => line.length > 0)
      .map((line) => {
        const cells = line.split(",");
        return {
          throttle_percent: parseCsvNumber(cells[0]),
          voltage_v: parseCsvNumber(cells[1]),
          current_a: parseCsvNumber(cells[2]),
          power_w: parseCsvNumber(cells[3]),
          rpm: parseCsvNumber(cells[4]),
          esc_temperature_c: parseCsvNumber(cells[5]),
          motor_temperature_c: parseCsvNumber(cells[6]),
          thrust_grams: parseCsvNumber(cells[7])
        };
      });
  };

  const setStudyStatus = (text) => {
    setText(ui.studyStatus, text);
  };

  const upsertStudyDataset = (dataset) => {
    const existingIndex = studyDatasets.findIndex((item) => item.id === dataset.id);
    if (existingIndex >= 0) {
      const previous = studyDatasets[existingIndex];
      studyDatasets[existingIndex] = {
        ...previous,
        ...dataset,
        visible: dataset.visible ?? previous.visible ?? true
      };
      return;
    }

    studyDatasets = [...studyDatasets, { ...dataset, visible: dataset.visible ?? true }];
  };

  const buildStudyLoadedSummary = () => {
    if (studyDatasets.length === 0) {
      return "No CSV loaded.";
    }

    const visibleCount = studyDatasets.filter((dataset) => dataset.visible !== false).length;
    const totalPoints = studyDatasets.reduce((sum, dataset) => sum + dataset.rows.length, 0);
    return `${studyDatasets.length} CSV loaded, ${visibleCount} visible, ${totalPoints} total points.`;
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
    setStopMotorTestButtonState(true);
    setDownloadTestButtonState(true);
  };

  const rebuildStudySelect = () => {
    if (!ui.studyFileSelect) {
      return;
    }

    const previousValue = ui.studyFileSelect.value;
    ui.studyFileSelect.innerHTML = "";

    if (savedTestFiles.length === 0) {
      const option = document.createElement("option");
      option.value = "";
      option.textContent = "No saved tests found";
      ui.studyFileSelect.appendChild(option);
      ui.studyFileSelect.disabled = true;
      if (ui.loadStudyButton) {
        ui.loadStudyButton.disabled = true;
      }
      return;
    }

    savedTestFiles.forEach((file) => {
      const option = document.createElement("option");
      option.value = file.name;
      option.textContent = `${file.name} (${file.size} B)`;
      ui.studyFileSelect.appendChild(option);
    });

    ui.studyFileSelect.disabled = false;
    ui.studyFileSelect.value = savedTestFiles.some((file) => file.name === previousValue)
      ? previousValue
      : savedTestFiles[0].name;

    if (ui.loadStudyButton) {
      ui.loadStudyButton.disabled = false;
    }
  };

  const refreshSavedTests = async () => {
    try {
      const response = await fetch("/api/tests", { cache: "no-store" });
      if (!response.ok) {
        throw new Error("HTTP status not ok");
      }

      const payload = await response.json();
      savedTestFiles = Array.isArray(payload.files) ? payload.files : [];
      rebuildStudySelect();

      if (savedTestFiles.length === 0) {
        setStudyStatus("No saved CSV files found in storage.");
      } else {
        setStudyStatus(studyDatasets.length > 0 ? buildStudyLoadedSummary() : "Choose a saved CSV and load it into the chart.");
      }
    } catch (error) {
      savedTestFiles = [];
      rebuildStudySelect();
      setStudyStatus("Could not read saved test list.");
    }
  };

  const setView = (view) => {
    links.forEach((link) => {
      link.classList.toggle("sidebar__nav-link--active", link.dataset.viewLink === view);
    });

    panes.forEach((pane) => {
      const paneView = pane.dataset.viewPane;
      const shouldShow = paneView === view || (paneView === "overview" && view === "testing");
      pane.hidden = !shouldShow;
    });

    setText(ui.viewTitle, viewTitles[view] || "Live overview");

    if (view === "study") {
      refreshSavedTests();
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
    updateChart(chartContexts.testing, cachedTestResults);

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
    updateChart(chartContexts.testing, []);

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

  const loadStudyFile = async () => {
    if (!ui.studyFileSelect || !ui.loadStudyButton || ui.studyFileSelect.disabled) {
      return;
    }

    const filename = ui.studyFileSelect.value;
    if (!filename) {
      setStudyStatus("Choose a saved CSV first.");
      return;
    }

    ui.loadStudyButton.disabled = true;
    setStudyStatus(`Loading ${filename}...`);

    try {
      const response = await fetch(`/api/test-file?name=${encodeURIComponent(filename)}`, { cache: "no-store" });
      if (!response.ok) {
        throw new Error("HTTP status not ok");
      }

      const csv = await response.text();
      const rows = parseSavedTestCsv(csv);
      upsertStudyDataset({
        id: buildStudyDatasetId("saved", filename),
        name: filename,
        rows,
        source: "saved",
        visible: true
      });
      updateStudyChart();
      setStudyStatus(`Loaded ${filename}. ${buildStudyLoadedSummary()}`);
    } catch (error) {
      setStudyStatus(`Could not load ${filename}.`);
    } finally {
      ui.loadStudyButton.disabled = ui.studyFileSelect.disabled;
    }
  };

  const openStudyFilePicker = () => {
    if (!ui.studyLocalFileInput) {
      return;
    }

    ui.studyLocalFileInput.value = "";
    ui.studyLocalFileInput.click();
  };

  const clearStudyGraph = () => {
    studyDatasets = [];
    updateStudyChart();
    setStudyStatus(savedTestFiles.length > 0
      ? "Choose a saved CSV and load it into the chart."
      : "No saved CSV files found in storage.");
  };

  const loadStudyFileFromPc = async (event) => {
    const files = Array.from(event.target.files || []);
    if (files.length === 0) {
      return;
    }

    setStudyStatus(`Loading ${files.length} CSV file${files.length === 1 ? "" : "s"} from PC...`);

    try {
      for (const file of files) {
        const csv = await file.text();
        const rows = parseSavedTestCsv(csv);
        upsertStudyDataset({
          id: buildStudyDatasetId("local", file.name),
          name: file.name,
          rows,
          source: "local",
          visible: true
        });
      }

      updateStudyChart();
      setStudyStatus(`Loaded ${files.length} file${files.length === 1 ? "" : "s"} from PC. ${buildStudyLoadedSummary()}`);
    } catch (error) {
      setStudyStatus("Could not load CSV file(s) from PC.");
    } finally {
      event.target.value = "";
    }
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
      console.info("WebSocket connected");
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

    socket.addEventListener("close", (event) => {
      const closeCode = typeof event.code === "number" ? event.code : 0;
      const closeReason = event.reason ? `: ${event.reason}` : "";
      console.warn(`WebSocket closed (${closeCode})${closeReason}`);
      socket = null;
      applyDisconnectedState();
      setHealth(ui.powerHealth, `WebSocket closed (${closeCode})`, "warn");
      startPolling();
      scheduleReconnect();
    });

    socket.addEventListener("error", (event) => {
      console.error("WebSocket error", event);
      setHealth(ui.powerHealth, "WebSocket error, retrying", "warn");
      startPolling();
      if (socket && socket.readyState !== WebSocket.CLOSING && socket.readyState !== WebSocket.CLOSED) {
        socket.close();
      }
    });
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

  setView("overview");
  applyDisconnectedState();
  syncStudySeriesControls();
  updateChart(chartContexts.testing, []);
  updateStudyChart();

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

  if (ui.loadStudyButton) {
    ui.loadStudyButton.addEventListener("click", loadStudyFile);
  }

  if (ui.openStudyFileButton) {
    ui.openStudyFileButton.addEventListener("click", openStudyFilePicker);
  }

  if (ui.clearStudyButton) {
    ui.clearStudyButton.addEventListener("click", clearStudyGraph);
  }

  if (ui.studyLocalFileInput) {
    ui.studyLocalFileInput.addEventListener("change", loadStudyFileFromPc);
  }

  if (ui.studyXAxisInputs.length > 0) {
    ui.studyXAxisInputs.forEach((input) => {
      input.addEventListener("change", () => {
        if (!input.checked) {
          return;
        }

        chartContexts.study.xAxisKey = input.value === "thrust_grams" ? "thrust_grams" : "throttle_percent";
        syncStudySeriesControls();
        updateStudyChart();
      });
    });
  }

  if (ui.studySeriesInputs.length > 0) {
    ui.studySeriesInputs.forEach((input) => {
      input.addEventListener("change", () => {
        chartContexts.study.visibleSeries[input.value] = input.checked;
        updateStudyChart();
      });
    });
  }
});


