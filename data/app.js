window.addEventListener("load", () => {
  const ui = {
    viewTitle: document.getElementById("view-title"),
    startMotorTestButton: document.getElementById("start-motor-test-button"),
    motorTestCoolingToggle: document.getElementById("motor-test-cooling-toggle"),
    stopMotorTestButton: document.getElementById("stop-motor-test-button"),
    downloadTestButton: document.getElementById("download-test-button"),
    motorTestDirectionInputs: Array.from(document.querySelectorAll('input[name="motor-test-direction"]')),
    studyFileSelect: document.getElementById("study-file-select"),
    loadStudyButton: document.getElementById("load-study-button"),
    openStudyFileButton: document.getElementById("open-study-file-button"),
    clearStudyButton: document.getElementById("clear-study-button"),
    studyLocalFileInput: document.getElementById("study-local-file-input"),
    studyStatus: document.getElementById("study-status"),
    studyXAxisInputs: Array.from(document.querySelectorAll('input[name="study-x-axis"]')),
    studySeriesInputs: Array.from(document.querySelectorAll('input[name="study-series"]')),
    studyThrustSeriesOption: document.getElementById("study-series-thrust")?.closest("label") || null,
    studyChartPlot: document.querySelector('[data-view-pane="study"] .chart-plot--study'),
    studyChartGrid: document.querySelector('[data-view-pane="study"] .chart-plot--study .chart-plot__grid'),
    studyFileVisibilityGroup: document.getElementById("study-file-visibility-group"),
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
      currentPath: document.getElementById("testing-chart-current-path"),
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
      rightTopCurrent: document.getElementById("testing-chart-right-top-current"),
      rightMidCurrent: document.getElementById("testing-chart-right-mid-current"),
      rightBottomCurrent: document.getElementById("testing-chart-right-bottom-current"),
      rightTopTemp: document.getElementById("testing-chart-right-top-temp"),
      rightMidTemp: document.getElementById("testing-chart-right-mid-temp"),
      rightBottomTemp: document.getElementById("testing-chart-right-bottom-temp")
    },
    study: {
      thrustLayer: document.getElementById("study-chart-thrust-layer"),
      powerLayer: document.getElementById("study-chart-power-layer"),
      currentLayer: document.getElementById("study-chart-current-layer"),
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
      rightTopCurrent: document.getElementById("study-chart-right-top-current"),
      rightMidCurrent: document.getElementById("study-chart-right-mid-current"),
      rightBottomCurrent: document.getElementById("study-chart-right-bottom-current"),
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
        current: true,
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
  let motorTestCoolingUpdatePending = false;
  let motorTestDirectionUpdatePending = false;
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

  const setScaleLabels = (topElement, midElement, bottomElement, minValue, maxValue, unit, digits = 0) => {
    const safeMin = Number.isFinite(minValue) ? minValue : 0;
    const safeMax = Number.isFinite(maxValue) ? maxValue : 0;
    setText(topElement, formatScaleValue(safeMax, unit, digits));
    setText(midElement, formatScaleValue((safeMin + safeMax) / 2, unit, digits));
    setText(bottomElement, formatScaleValue(safeMin, unit, digits));
  };

  const formatAxisTick = (value, unit = "", digits = 0) => {
    if (!Number.isFinite(value)) {
      return "--";
    }

    const suffix = unit ? ` ${unit}` : "";
    return `${Number(value).toFixed(digits)}${suffix}`;
  };

  const getSeriesRange = (rows, valueKey, { includeZero = true, fallbackMin = 0, fallbackMax = 0 } = {}) => {
    if (!Array.isArray(rows) || rows.length === 0) {
      return { min: fallbackMin, max: fallbackMax };
    }

    let min = Number.POSITIVE_INFINITY;
    let max = Number.NEGATIVE_INFINITY;

    rows.forEach((row) => {
      const value = Number(row?.[valueKey]);
      if (!Number.isFinite(value)) {
        return;
      }

      min = Math.min(min, value);
      max = Math.max(max, value);
    });

    if (!Number.isFinite(min) || !Number.isFinite(max)) {
      return { min: fallbackMin, max: fallbackMax };
    }

    if (includeZero) {
      min = Math.min(min, 0);
      max = Math.max(max, 0);
    }

    if (min === max) {
      if (min === 0) {
        max = fallbackMax !== fallbackMin ? fallbackMax : 1;
      } else if (min > 0) {
        min = 0;
      } else {
        max = 0;
      }
    }

    return { min, max };
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
    const powerRange = getSeriesRange(rows, "power_w", { fallbackMax: 1 });
    const rpmRange = getSeriesRange(rows, "rpm", { fallbackMax: 1 });
    const thrustRange = getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 });
    const currentRange = getSeriesRange(rows, "current_a", { fallbackMax: 1 });
    const tempRange = getSeriesRange(rows, "motor_temperature_c", { fallbackMax: 1 });
    const hideThrustScale = context.xAxisKey === "thrust_grams";

    setScaleLabels(context.leftTopPower, context.leftMidPower, context.leftBottomPower, powerRange.min, powerRange.max, "W", 0);
    setScaleLabels(context.leftTopRpm, context.leftMidRpm, context.leftBottomRpm, rpmRange.min, rpmRange.max, "rpm", 0);
    setScaleLabels(context.rightTopThrust, context.rightMidThrust, context.rightBottomThrust, thrustRange.min, thrustRange.max, "g", 0);
    setScaleLabels(context.rightTopCurrent, context.rightMidCurrent, context.rightBottomCurrent, currentRange.min, currentRange.max, "A", 2);
    setScaleLabels(context.rightTopTemp, context.rightMidTemp, context.rightBottomTemp, tempRange.min, tempRange.max, "C", 1);

    [context.rightTopThrust, context.rightMidThrust, context.rightBottomThrust].forEach((element) => {
      if (element) {
        element.style.display = hideThrustScale ? "none" : "";
      }
    });
  };

  const updateXAxis = (context, rows) => {
    if (!context.xLabel || !Array.isArray(context.xTicks) || context.xTicks.length === 0) {
      return;
    }

    const isThrustAxis = context.xAxisKey === "thrust_grams";
    const unit = isThrustAxis ? "g" : "";
    const digits = isThrustAxis ? 0 : 0;
    const thrustRange = getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 });
    const minValue = isThrustAxis ? thrustRange.min : 0;
    const maxValue = isThrustAxis ? thrustRange.max : 100;

    setText(context.xLabel, isThrustAxis ? "Thrust" : "Throttle %");
    context.xTicks.forEach((tick, index) => {
      const ratio = context.xTicks.length === 1 ? 0 : index / (context.xTicks.length - 1);
      setText(tick, formatAxisTick(minValue + (maxValue - minValue) * ratio, unit, digits));
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
        if (!Number.isFinite(x)) {
          return null;
        }

        return {
          x,
          y: Number.isFinite(y) ? y : 0
        };
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

    const derivedXRange = points.reduce((range, point) => ({
      min: Math.min(range.min, point.x),
      max: Math.max(range.max, point.x)
    }), { min: Number.POSITIVE_INFINITY, max: Number.NEGATIVE_INFINITY });
    const derivedYRange = points.reduce((range, point) => ({
      min: Math.min(range.min, point.y),
      max: Math.max(range.max, point.y)
    }), { min: Number.POSITIVE_INFINITY, max: Number.NEGATIVE_INFINITY });
    const safeMinX = Number.isFinite(axisMax.minX) ? axisMax.minX : (Number.isFinite(derivedXRange.min) ? derivedXRange.min : 0);
    const safeMaxX = Number.isFinite(axisMax.maxX) ? axisMax.maxX : (Number.isFinite(derivedXRange.max) ? derivedXRange.max : 1);
    const safeMinY = Number.isFinite(axisMax.minY) ? axisMax.minY : (Number.isFinite(derivedYRange.min) ? derivedYRange.min : 0);
    const safeMaxY = Number.isFinite(axisMax.maxY) ? axisMax.maxY : (Number.isFinite(derivedYRange.max) ? derivedYRange.max : 1);
    const xSpan = safeMaxX !== safeMinX ? (safeMaxX - safeMinX) : 1;
    const ySpan = safeMaxY !== safeMinY ? (safeMaxY - safeMinY) : 1;
    const plotPaddingX = 1.5;
    const plotPaddingY = 1.5;
    const plotWidth = 100 - plotPaddingX * 2;
    const plotHeight = 100 - plotPaddingY * 2;

    return points
      .map((point, index) => {
        const px = plotPaddingX + ((point.x - safeMinX) / xSpan) * plotWidth;
        const py = 100 - plotPaddingY - ((point.y - safeMinY) / ySpan) * plotHeight;
        const clampedX = Math.max(0, Math.min(100, px));
        const clampedY = Math.max(0, Math.min(100, py));
        return `${index === 0 ? "M" : "L"}${clampedX.toFixed(2)},${clampedY.toFixed(2)}`;
      })
      .join(" ");
  };

  const getChartAxisBounds = (rows, xAxisKey) => {
    const xRange = xAxisKey === "thrust_grams"
      ? getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 })
      : { min: 0, max: 100 };
    const thrustRange = getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 });
    const powerRange = getSeriesRange(rows, "power_w", { fallbackMin: 0, fallbackMax: 1 });
    const currentRange = getSeriesRange(rows, "current_a", { fallbackMin: 0, fallbackMax: 1 });
    const rpmRange = getSeriesRange(rows, "rpm", { fallbackMin: 0, fallbackMax: 1 });
    const tempRange = getSeriesRange(rows, "motor_temperature_c", { fallbackMin: 0, fallbackMax: 1 });

    return {
      x: xRange,
      thrust: thrustRange,
      power: powerRange,
      current: currentRange,
      rpm: rpmRange,
      temp: tempRange
    };
  };

  const updateChart = (context, rows) => {
    updateChartScales(context, rows);
    updateXAxis(context, rows);
    const xAxisKey = context.xAxisKey || "throttle_percent";
    const visibleSeries = context.visibleSeries || {};
    const hideThrustSeries = xAxisKey === "thrust_grams";
    const axisBounds = getChartAxisBounds(rows, xAxisKey);

    if (context.thrustPath) {
      context.thrustPath.setAttribute("d", buildChartPath(rows, "thrust_grams", xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.thrust.min,
        maxY: axisBounds.thrust.max
      }));
      context.thrustPath.parentElement.style.display = hideThrustSeries || visibleSeries.thrust === false ? "none" : "";
    }

    if (context.powerPath) {
      context.powerPath.setAttribute("d", buildChartPath(rows, "power_w", xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.power.min,
        maxY: axisBounds.power.max
      }));
      context.powerPath.parentElement.style.display = visibleSeries.power === false ? "none" : "";
    }

    if (context.currentPath) {
      context.currentPath.setAttribute("d", buildChartPath(rows, "current_a", xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.current.min,
        maxY: axisBounds.current.max
      }));
      context.currentPath.parentElement.style.display = visibleSeries.current === false ? "none" : "";
    }

    if (context.rpmPath) {
      context.rpmPath.setAttribute("d", buildChartPath(rows, "rpm", xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.rpm.min,
        maxY: axisBounds.rpm.max
      }));
      context.rpmPath.parentElement.style.display = visibleSeries.rpm === false ? "none" : "";
    }

    if (context.tempPath) {
      context.tempPath.setAttribute("d", buildChartPath(rows, "motor_temperature_c", xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.temp.min,
        maxY: axisBounds.temp.max
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

    syncStudyFilePickerHeight();
  };

  const syncStudyFilePickerHeight = () => {
    if (!ui.studyFileVisibilityGroup) {
      return;
    }

    const gridHeight = ui.studyChartGrid?.clientHeight || 0;
    const plotHeight = ui.studyChartPlot?.clientHeight || 0;
    const maxHeight = gridHeight > 0 ? gridHeight : plotHeight;

    if (maxHeight > 0) {
      ui.studyFileVisibilityGroup.style.setProperty("--study-file-picker-max-height", `${Math.floor(maxHeight)}px`);
    } else {
      ui.studyFileVisibilityGroup.style.removeProperty("--study-file-picker-max-height");
    }
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
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.thrust.min,
        maxY: axisBounds.thrust.max
      });
      context.thrustLayer.style.display = hideThrustSeries || visibleSeries.thrust === false ? "none" : "";
    }

    if (context.powerLayer) {
      context.powerLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "power_w", xAxisKey, "chart-line--power", {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.power.min,
        maxY: axisBounds.power.max
      });
      context.powerLayer.style.display = visibleSeries.power === false ? "none" : "";
    }

    if (context.currentLayer) {
      context.currentLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "current_a", xAxisKey, "chart-line--current", {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.current.min,
        maxY: axisBounds.current.max
      });
      context.currentLayer.style.display = visibleSeries.current === false ? "none" : "";
    }

    if (context.rpmLayer) {
      context.rpmLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "rpm", xAxisKey, "chart-line--rpm", {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.rpm.min,
        maxY: axisBounds.rpm.max
      });
      context.rpmLayer.style.display = visibleSeries.rpm === false ? "none" : "";
    }

    if (context.tempLayer) {
      context.tempLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, "motor_temperature_c", xAxisKey, "chart-line--temp", {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.temp.min,
        maxY: axisBounds.temp.max
      });
      context.tempLayer.style.display = visibleSeries.temp === false ? "none" : "";
    }

    renderStudyFileControls();
    syncStudyFilePickerHeight();
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

  const setMotorTestCoolingToggleState = (disabled = false) => {
    if (ui.motorTestCoolingToggle) {
      ui.motorTestCoolingToggle.disabled = disabled;
    }
  };

  const setMotorTestDirectionState = (disabled = false) => {
    ui.motorTestDirectionInputs.forEach((input) => {
      input.disabled = disabled;
    });
  };

  const getSelectedMotorTestDirection = () => {
    const selected = ui.motorTestDirectionInputs.find((input) => input.checked);
    return selected?.value === "pusher" ? "pusher" : "puller";
  };

  const setSelectedMotorTestDirection = (direction) => {
    const normalized = direction === "pusher" ? "pusher" : "puller";
    ui.motorTestDirectionInputs.forEach((input) => {
      input.checked = input.value === normalized;
    });
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
    setMotorTestCoolingToggleState(motorTestCoolingUpdatePending);
    setMotorTestDirectionState(motorTestDirectionUpdatePending);
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
      requestAnimationFrame(syncStudyFilePickerHeight);
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
      setMotorTestCoolingToggleState(true);
      setMotorTestDirectionState(true);
    } else {
      lastKnownTestRunning = false;
      motorTestPending = false;
      setMotorTestButtonState("Start test", false);
      setStopMotorTestButtonState(true);
      setDownloadTestButtonState(resultCount === 0);
      setMotorTestCoolingToggleState(motorTestCoolingUpdatePending);
      setMotorTestDirectionState(motorTestDirectionUpdatePending);
    }

    if (ui.motorTestCoolingToggle) {
      ui.motorTestCoolingToggle.checked = Boolean(data.motor_test_cooling_enabled);
    }

    setSelectedMotorTestDirection(Boolean(data.motor_test_pusher_mode) ? "pusher" : "puller");

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
    setMotorTestCoolingToggleState(true);
    cachedTestResults = [];
    cachedTestResultCount = 0;
    updateChart(chartContexts.testing, []);

    try {
      const direction = getSelectedMotorTestDirection();
      const body = new URLSearchParams({ cmd: direction === "pusher" ? "motor test pusher" : "motor test puller" });
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
      setMotorTestCoolingToggleState(motorTestCoolingUpdatePending);
      setMotorTestDirectionState(motorTestDirectionUpdatePending);
      setHealth(ui.powerHealth, "Failed to start motor test", "error");
    }
  };

  const updateMotorTestDirection = async () => {
    if (motorTestPending || lastKnownTestRunning) {
      return;
    }

    const direction = getSelectedMotorTestDirection();
    motorTestDirectionUpdatePending = true;
    setMotorTestDirectionState(true);

    try {
      const body = new URLSearchParams({
        cmd: direction === "pusher" ? "motor test mode pusher" : "motor test mode puller"
      });
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

      await pollStatus();
    } catch (error) {
      setSelectedMotorTestDirection(direction === "pusher" ? "puller" : "pusher");
      setHealth(ui.powerHealth, "Failed to update thrust direction", "error");
    } finally {
      motorTestDirectionUpdatePending = false;
      setMotorTestDirectionState(lastKnownTestRunning);
    }
  };

  const updateMotorTestCooling = async () => {
    if (!ui.motorTestCoolingToggle || motorTestPending || lastKnownTestRunning) {
      return;
    }

    const enabled = ui.motorTestCoolingToggle.checked;
    motorTestCoolingUpdatePending = true;
    setMotorTestCoolingToggleState(true);

    try {
      const body = new URLSearchParams({
        cmd: enabled ? "motor test cooling on" : "motor test cooling off"
      });
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

      await pollStatus();
    } catch (error) {
      ui.motorTestCoolingToggle.checked = !enabled;
      setHealth(ui.powerHealth, "Failed to update cooldown flag", "error");
    } finally {
      motorTestCoolingUpdatePending = false;
      setMotorTestCoolingToggleState(lastKnownTestRunning);
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
  window.addEventListener("resize", syncStudyFilePickerHeight);

  startPolling();
  connectWebSocket();

  if (ui.startMotorTestButton) {
    ui.startMotorTestButton.addEventListener("click", runMotorTest);
  }

  if (ui.motorTestCoolingToggle) {
    ui.motorTestCoolingToggle.addEventListener("change", updateMotorTestCooling);
  }

  ui.motorTestDirectionInputs.forEach((input) => {
    input.addEventListener("change", updateMotorTestDirection);
  });

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


