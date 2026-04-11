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
    studyEfficiencySummaryGroup: document.getElementById("study-efficiency-summary-group"),
    testingEfficiencySummaryEmpty: document.getElementById("testing-efficiency-summary-empty"),
    testingEfficiencySummaryList: document.getElementById("testing-efficiency-summary-list"),
    studyEfficiencySummaryEmpty: document.getElementById("study-efficiency-summary-empty"),
    studyEfficiencySummaryList: document.getElementById("study-efficiency-summary-list"),
    overviewThrottle: document.getElementById("overview-throttle-value"),
    overviewThrottleHealth: document.getElementById("overview-throttle-health"),
    overviewMotorControlCard: document.getElementById("overview-motor-control-card"),
    overviewMotorSetValue: document.getElementById("overview-motor-set-value"),
    overviewMotorSetButton: document.getElementById("overview-motor-set-button"),
    overviewMotorStopButton: document.getElementById("overview-motor-stop-button"),
    configEscPolesValue: document.getElementById("config-esc-poles-value"),
    configEscPolesButton: document.getElementById("config-esc-poles-button"),
    configEscReverseButton: document.getElementById("config-esc-reverse-button"),
    configCurrentLowValue: document.getElementById("config-current-low-value"),
    configCurrentLowButton: document.getElementById("config-current-low-button"),
    configCurrentHighValue: document.getElementById("config-current-high-value"),
    configCurrentHighButton: document.getElementById("config-current-high-button"),
    configVoltageLowValue: document.getElementById("config-voltage-low-value"),
    configVoltageLowButton: document.getElementById("config-voltage-low-button"),
    configVoltageHighValue: document.getElementById("config-voltage-high-value"),
    configVoltageHighButton: document.getElementById("config-voltage-high-button"),
    configScaleTareButton: document.getElementById("config-scale-tare-button"),
    configScaleCalibrationValue: document.getElementById("config-scale-calibration-value"),
    configScaleCalibrationButton: document.getElementById("config-scale-calibration-button"),
    configScaleFactorValue: document.getElementById("config-scale-factor-value"),
    configSafetyCurrentHiValue: document.getElementById("config-safety-current-hi-value"),
    configSafetyCurrentHiButton: document.getElementById("config-safety-current-hi-button"),
    configSafetyCurrentHiHiValue: document.getElementById("config-safety-current-hihi-value"),
    configSafetyCurrentHiHiButton: document.getElementById("config-safety-current-hihi-button"),
    configSafetyMotorTempHiValue: document.getElementById("config-safety-motor-temp-hi-value"),
    configSafetyMotorTempHiButton: document.getElementById("config-safety-motor-temp-hi-button"),
    configSafetyMotorTempHiHiValue: document.getElementById("config-safety-motor-temp-hihi-value"),
    configSafetyMotorTempHiHiButton: document.getElementById("config-safety-motor-temp-hihi-button"),
    configSafetyEscTempHiValue: document.getElementById("config-safety-esc-temp-hi-value"),
    configSafetyEscTempHiButton: document.getElementById("config-safety-esc-temp-hi-button"),
    configSafetyEscTempHiHiValue: document.getElementById("config-safety-esc-temp-hihi-value"),
    configSafetyEscTempHiHiButton: document.getElementById("config-safety-esc-temp-hihi-button"),
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
      plot: document.querySelector('[data-view-pane="testing"] .chart-plot--testing'),
      thrustPath: document.getElementById("testing-chart-thrust-path"),
      thrustLayer: document.getElementById("testing-chart-thrust-path")?.closest(".chart-line-layer") || null,
      efficiencyPath: document.getElementById("testing-chart-efficiency-path"),
      efficiencyLayer: document.getElementById("testing-chart-efficiency-path")?.closest(".chart-line-layer") || null,
      voltagePath: document.getElementById("testing-chart-voltage-path"),
      voltageLayer: document.getElementById("testing-chart-voltage-path")?.closest(".chart-line-layer") || null,
      powerPath: document.getElementById("testing-chart-power-path"),
      powerLayer: document.getElementById("testing-chart-power-path")?.closest(".chart-line-layer") || null,
      currentPath: document.getElementById("testing-chart-current-path"),
      currentLayer: document.getElementById("testing-chart-current-path")?.closest(".chart-line-layer") || null,
      rpmPath: document.getElementById("testing-chart-rpm-path"),
      rpmLayer: document.getElementById("testing-chart-rpm-path")?.closest(".chart-line-layer") || null,
      tempPath: document.getElementById("testing-chart-temp-path"),
      tempLayer: document.getElementById("testing-chart-temp-path")?.closest(".chart-line-layer") || null,
      leftVoltageTicks: [
        document.getElementById("testing-chart-left-top-voltage"),
        document.getElementById("testing-chart-left-upper-mid-voltage"),
        document.getElementById("testing-chart-left-mid-voltage"),
        document.getElementById("testing-chart-left-lower-mid-voltage"),
        document.getElementById("testing-chart-left-bottom-voltage")
      ],
      leftPowerTicks: [
        document.getElementById("testing-chart-left-top-power"),
        document.getElementById("testing-chart-left-upper-mid-power"),
        document.getElementById("testing-chart-left-mid-power"),
        document.getElementById("testing-chart-left-lower-mid-power"),
        document.getElementById("testing-chart-left-bottom-power")
      ],
      leftRpmTicks: [
        document.getElementById("testing-chart-left-top-rpm"),
        document.getElementById("testing-chart-left-upper-mid-rpm"),
        document.getElementById("testing-chart-left-mid-rpm"),
        document.getElementById("testing-chart-left-lower-mid-rpm"),
        document.getElementById("testing-chart-left-bottom-rpm")
      ],
      rightThrustTicks: [
        document.getElementById("testing-chart-right-top-thrust"),
        document.getElementById("testing-chart-right-upper-mid-thrust"),
        document.getElementById("testing-chart-right-mid-thrust"),
        document.getElementById("testing-chart-right-lower-mid-thrust"),
        document.getElementById("testing-chart-right-bottom-thrust")
      ],
      rightEfficiencyTicks: [
        document.getElementById("testing-chart-right-top-efficiency"),
        document.getElementById("testing-chart-right-upper-mid-efficiency"),
        document.getElementById("testing-chart-right-mid-efficiency"),
        document.getElementById("testing-chart-right-lower-mid-efficiency"),
        document.getElementById("testing-chart-right-bottom-efficiency")
      ],
      rightCurrentTicks: [
        document.getElementById("testing-chart-right-top-current"),
        document.getElementById("testing-chart-right-upper-mid-current"),
        document.getElementById("testing-chart-right-mid-current"),
        document.getElementById("testing-chart-right-lower-mid-current"),
        document.getElementById("testing-chart-right-bottom-current")
      ],
      rightTempTicks: [
        document.getElementById("testing-chart-right-top-temp"),
        document.getElementById("testing-chart-right-upper-mid-temp"),
        document.getElementById("testing-chart-right-mid-temp"),
        document.getElementById("testing-chart-right-lower-mid-temp"),
        document.getElementById("testing-chart-right-bottom-temp")
      ]
    },
    study: {
      plot: document.querySelector('[data-view-pane="study"] .chart-plot--study'),
      thrustLayer: document.getElementById("study-chart-thrust-layer"),
      efficiencyLayer: document.getElementById("study-chart-efficiency-layer"),
      voltageLayer: document.getElementById("study-chart-voltage-layer"),
      powerLayer: document.getElementById("study-chart-power-layer"),
      currentLayer: document.getElementById("study-chart-current-layer"),
      rpmLayer: document.getElementById("study-chart-rpm-layer"),
      tempLayer: document.getElementById("study-chart-temp-layer"),
      leftVoltageTicks: [
        document.getElementById("study-chart-left-top-voltage"),
        document.getElementById("study-chart-left-upper-mid-voltage"),
        document.getElementById("study-chart-left-mid-voltage"),
        document.getElementById("study-chart-left-lower-mid-voltage"),
        document.getElementById("study-chart-left-bottom-voltage")
      ],
      leftPowerTicks: [
        document.getElementById("study-chart-left-top-power"),
        document.getElementById("study-chart-left-upper-mid-power"),
        document.getElementById("study-chart-left-mid-power"),
        document.getElementById("study-chart-left-lower-mid-power"),
        document.getElementById("study-chart-left-bottom-power")
      ],
      leftRpmTicks: [
        document.getElementById("study-chart-left-top-rpm"),
        document.getElementById("study-chart-left-upper-mid-rpm"),
        document.getElementById("study-chart-left-mid-rpm"),
        document.getElementById("study-chart-left-lower-mid-rpm"),
        document.getElementById("study-chart-left-bottom-rpm")
      ],
      rightThrustTicks: [
        document.getElementById("study-chart-right-top-thrust"),
        document.getElementById("study-chart-right-upper-mid-thrust"),
        document.getElementById("study-chart-right-mid-thrust"),
        document.getElementById("study-chart-right-lower-mid-thrust"),
        document.getElementById("study-chart-right-bottom-thrust")
      ],
      rightEfficiencyTicks: [
        document.getElementById("study-chart-right-top-efficiency"),
        document.getElementById("study-chart-right-upper-mid-efficiency"),
        document.getElementById("study-chart-right-mid-efficiency"),
        document.getElementById("study-chart-right-lower-mid-efficiency"),
        document.getElementById("study-chart-right-bottom-efficiency")
      ],
      rightCurrentTicks: [
        document.getElementById("study-chart-right-top-current"),
        document.getElementById("study-chart-right-upper-mid-current"),
        document.getElementById("study-chart-right-mid-current"),
        document.getElementById("study-chart-right-lower-mid-current"),
        document.getElementById("study-chart-right-bottom-current")
      ],
      rightTempTicks: [
        document.getElementById("study-chart-right-top-temp"),
        document.getElementById("study-chart-right-upper-mid-temp"),
        document.getElementById("study-chart-right-mid-temp"),
        document.getElementById("study-chart-right-lower-mid-temp"),
        document.getElementById("study-chart-right-bottom-temp")
      ],
      xLabel: document.querySelector('[data-view-pane="study"] .chart-plot__xlabel'),
      xTicks: [
        document.getElementById("study-chart-tick-0"),
        document.getElementById("study-chart-tick-1"),
        document.getElementById("study-chart-tick-2"),
        document.getElementById("study-chart-tick-3"),
        document.getElementById("study-chart-tick-4"),
        document.getElementById("study-chart-tick-5"),
        document.getElementById("study-chart-tick-6"),
        document.getElementById("study-chart-tick-7"),
        document.getElementById("study-chart-tick-8")
      ],
      xAxisKey: "thrust_grams",
      visibleSeries: {
        thrust: true,
        efficiency: true,
        voltage: true,
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
    study: "Study",
    configuration: "Configuration"
  };

  let socket = null;
  let reconnectTimer = null;
  let pollTimer = null;
  let overviewMotorCommandPending = false;
  let configurationCommandPending = false;
  let motorTestPending = false;
  let motorTestCoolingUpdatePending = false;
  let motorTestDirectionUpdatePending = false;
  let lastKnownTestRunning = false;
  let cachedTestResults = [];
  let cachedTestResultCount = 0;
  let savedTestFiles = [];
  let studyDatasets = [];

  const calculateThrustEfficiency = (thrustGrams, powerWatts) => {
    const thrust = Number(thrustGrams);
    const power = Number(powerWatts);
    if (
      !Number.isFinite(thrust)
      || !Number.isFinite(power)
      || thrust <= 0
      || power <= 1
    ) {
      return Number.NaN;
    }

    return thrust / power;
  };

  const normalizeChartRow = (row) => ({
    ...row,
    thrust_efficiency_g_per_w: calculateThrustEfficiency(row?.thrust_grams, row?.power_w)
  });

  const normalizeChartRows = (rows) => Array.isArray(rows)
    ? rows.map((row) => normalizeChartRow(row))
    : [];

  const chartSeriesMeta = {
    thrust: { valueKey: "thrust_grams", unit: "g", digits: 0, color: "#244d74", className: "chart-line--thrust" },
    efficiency: { valueKey: "thrust_efficiency_g_per_w", unit: "gr/W", digits: 2, color: "#b06a00", className: "chart-line--efficiency" },
    voltage: { valueKey: "voltage_v", unit: "V", digits: 2, color: "#8a4fff", className: "chart-line--voltage" },
    power: { valueKey: "power_w", unit: "W", digits: 0, color: "#546fe5", className: "chart-line--power" },
    current: { valueKey: "current_a", unit: "A", digits: 2, color: "#1aa36f", className: "chart-line--current" },
    rpm: { valueKey: "rpm", unit: "rpm", digits: 0, color: "#d94141", className: "chart-line--rpm" },
    temp: { valueKey: "motor_temperature_c", unit: "C", digits: 1, color: "#ff7c5c", className: "chart-line--temp" }
  };

  const chartXAxisMeta = {
    throttle_percent: { unit: "%", digits: 0 },
    thrust_grams: { unit: "g", digits: 0 }
  };

  const calculateAverageEfficiencyForThrottleBand = (rows, minThrottle = 40, maxThrottle = 80) => {
    if (!Array.isArray(rows) || rows.length === 0) {
      return null;
    }

    const matchingValues = rows
      .map((row) => ({
        throttle: Number(row?.throttle_percent),
        efficiency: Number(row?.thrust_efficiency_g_per_w)
      }))
      .filter(({ throttle, efficiency }) => (
        Number.isFinite(throttle)
        && Number.isFinite(efficiency)
        && throttle >= minThrottle
        && throttle <= maxThrottle
      ))
      .map(({ efficiency }) => efficiency);

    if (matchingValues.length === 0) {
      return null;
    }

    const sum = matchingValues.reduce((total, value) => total + value, 0);
    return {
      average: sum / matchingValues.length,
      sampleCount: matchingValues.length
    };
  };

  const getMaxThrottle = (rows) => rows.reduce((maxValue, row) => {
    const throttle = Number(row?.throttle_percent);
    return Number.isFinite(throttle) ? Math.max(maxValue, throttle) : maxValue;
  }, Number.NEGATIVE_INFINITY);

  const renderEfficiencySummaryList = (listElement, emptyElement, items, emptyMessage) => {
    if (!listElement || !emptyElement) {
      return;
    }

    if (!Array.isArray(items) || items.length === 0) {
      listElement.innerHTML = "";
      emptyElement.hidden = false;
      setText(emptyElement, emptyMessage);
      return;
    }

    emptyElement.hidden = true;
    listElement.innerHTML = items
      .map((item) => `
        <article class="summary-metric">
          <span class="summary-metric__label">${escapeHtml(item.label)}</span>
          <strong class="summary-metric__value summary-metric__value--efficiency">${escapeHtml(formatNumber(item.average, "gr/W", 2))}</strong>
        </article>
      `)
      .join("");
  };

  const updateTestingEfficiencySummary = (rows, isTestRunning) => {
    const maxThrottle = getMaxThrottle(rows);
    if (isTestRunning) {
      renderEfficiencySummaryList(
        ui.testingEfficiencySummaryList,
        ui.testingEfficiencySummaryEmpty,
        [],
        "Results appear after the test finishes and reaches 80% throttle."
      );
      return;
    }

    if (!Number.isFinite(maxThrottle) || maxThrottle < 80) {
      renderEfficiencySummaryList(
        ui.testingEfficiencySummaryList,
        ui.testingEfficiencySummaryEmpty,
        [],
        "Results will appear once a completed test reaches at least 80% throttle."
      );
      return;
    }

    const summary = calculateAverageEfficiencyForThrottleBand(rows, 40, 80);
    renderEfficiencySummaryList(
      ui.testingEfficiencySummaryList,
      ui.testingEfficiencySummaryEmpty,
      summary ? [{ label: "Completed test", ...summary }] : [],
      "No valid efficiency points were found between 40% and 80% throttle."
    );
  };

  const updateStudyEfficiencySummary = (datasets) => {
    const visibleDatasets = Array.isArray(datasets)
      ? datasets.filter((dataset) => dataset && dataset.visible !== false)
      : [];

    const items = visibleDatasets
      .map((dataset) => {
        const summary = calculateAverageEfficiencyForThrottleBand(dataset.rows, 40, 80);
        if (!summary) {
          return null;
        }

        return {
          label: dataset.name || "CSV",
          average: summary.average,
          sampleCount: summary.sampleCount
        };
      })
      .filter(Boolean);

    renderEfficiencySummaryList(
      ui.studyEfficiencySummaryList,
      ui.studyEfficiencySummaryEmpty,
      items,
      visibleDatasets.length === 0
        ? "Load visible CSV data to compare average efficiency."
        : "No visible CSV has valid efficiency points between 40% and 80% throttle."
    );
  };

  const setText = (element, value) => {
    if (element) {
      element.textContent = value;
    }
  };

  const setInputValueIfIdle = (element, value) => {
    if (!element || document.activeElement === element) {
      return;
    }

    element.value = value;
  };

  const setFixedInputValueIfIdle = (element, value, digits) => {
    if (!element || document.activeElement === element) {
      return;
    }

    const numericValue = Number(value);
    element.value = Number.isFinite(numericValue) ? numericValue.toFixed(digits) : "";
  };

  const setThresholdInputValueIfIdle = (element, value, digits) => {
    if (!element || document.activeElement === element) {
      return;
    }

    const numericValue = Number(value);
    element.value = Number.isFinite(numericValue) && numericValue >= 0
      ? numericValue.toFixed(digits)
      : "";
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

  const describeSafetyState = (state, normalText, hiText, hihiText, unavailableText) => {
    if (state === "hihi") {
      return { text: hihiText, severity: "error" };
    }
    if (state === "hi") {
      return { text: hiText, severity: "warn" };
    }
    if (state === "normal") {
      return { text: normalText, severity: "ok" };
    }
    return { text: unavailableText, severity: "warn" };
  };

  const formatNumber = (value, unit, digits = 2) => {
    if (value === null || value === undefined || Number.isNaN(Number(value))) {
      return "--";
    }

    return `${Number(value).toFixed(digits)} ${unit}`;
  };

  const formatAdaptiveNumber = (value, unit) => {
    const numericValue = Number(value);
    if (value === null || value === undefined || Number.isNaN(numericValue)) {
      return "--";
    }

    const magnitude = Math.abs(numericValue);
    let digits = 0;
    if (magnitude < 10) {
      digits = 2;
    } else if (magnitude < 100) {
      digits = 1;
    }

    return `${numericValue.toFixed(digits)} ${unit}`;
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

  const setScaleLabels = (elements, minValue, maxValue, unit, digits = 0) => {
    const safeMin = Number.isFinite(minValue) ? minValue : 0;
    const safeMax = Number.isFinite(maxValue) ? maxValue : 0;
    const ticks = Array.isArray(elements) ? elements : [];
    ticks.forEach((element, index) => {
      const ratio = ticks.length <= 1 ? 0 : 1 - (index / (ticks.length - 1));
      setText(element, formatScaleValue(safeMin + (safeMax - safeMin) * ratio, unit, digits));
    });
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
    const voltageRange = getSeriesRange(rows, "voltage_v", { fallbackMax: 1 });
    const powerRange = getSeriesRange(rows, "power_w", { fallbackMax: 1 });
    const rpmRange = getSeriesRange(rows, "rpm", { fallbackMax: 1 });
    const thrustRange = getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 });
    const efficiencyRange = getSeriesRange(rows, "thrust_efficiency_g_per_w", { fallbackMin: 0, fallbackMax: 1 });
    const currentRange = getSeriesRange(rows, "current_a", { fallbackMax: 1 });
    const tempRange = getSeriesRange(rows, "motor_temperature_c", { fallbackMax: 1 });
    const hideThrustScale = context.xAxisKey === "thrust_grams";

    setScaleLabels(context.leftVoltageTicks, voltageRange.min, voltageRange.max, "V", 2);
    setScaleLabels(context.leftPowerTicks, powerRange.min, powerRange.max, "W", 0);
    setScaleLabels(context.leftRpmTicks, rpmRange.min, rpmRange.max, "rpm", 0);
    setScaleLabels(context.rightThrustTicks, thrustRange.min, thrustRange.max, "g", 0);
    setScaleLabels(context.rightEfficiencyTicks, efficiencyRange.min, efficiencyRange.max, "gr/W", 2);
    setScaleLabels(context.rightCurrentTicks, currentRange.min, currentRange.max, "A", 2);
    setScaleLabels(context.rightTempTicks, tempRange.min, tempRange.max, "C", 1);

    context.rightThrustTicks.forEach((element) => {
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

  const formatTooltipValue = (value, unit, digits = 0) => {
    if (!Number.isFinite(value)) {
      return "--";
    }

    return `${Number(value).toFixed(digits)} ${unit}`;
  };

  const projectChartPoint = (point, axisMax) => {
    const safeMinX = Number.isFinite(axisMax.minX) ? axisMax.minX : 0;
    const safeMaxX = Number.isFinite(axisMax.maxX) ? axisMax.maxX : 1;
    const safeMinY = Number.isFinite(axisMax.minY) ? axisMax.minY : 0;
    const safeMaxY = Number.isFinite(axisMax.maxY) ? axisMax.maxY : 1;
    const xSpan = safeMaxX !== safeMinX ? (safeMaxX - safeMinX) : 1;
    const ySpan = safeMaxY !== safeMinY ? (safeMaxY - safeMinY) : 1;
    const plotPaddingX = 1.5;
    const plotPaddingY = 1.5;
    const plotWidth = 100 - plotPaddingX * 2;
    const plotHeight = 100 - plotPaddingY * 2;
    const px = plotPaddingX + ((point.x - safeMinX) / xSpan) * plotWidth;
    const py = 100 - plotPaddingY - ((point.y - safeMinY) / ySpan) * plotHeight;

    return {
      ...point,
      px: Math.max(0, Math.min(100, px)),
      py: Math.max(0, Math.min(100, py))
    };
  };

  const buildChartGeometry = (rows, valueKey, xKey = "throttle_percent", axisMax = {}) => {
    if (!Array.isArray(rows) || rows.length === 0) {
      return {
        path: "M0,100 L100,100",
        points: []
      };
    }

    const points = rows
      .map((row, index) => {
        const x = Number(row?.[xKey]);
        const y = Number(row?.[valueKey]);
        if (!Number.isFinite(x)) {
          return null;
        }

        return {
          index,
          x,
          y: Number.isFinite(y) ? y : 0,
          isRenderable: Number.isFinite(y)
        };
      })
      .filter(Boolean);

    if (points.length === 0) {
      return {
        path: "M0,100 L100,100",
        points: []
      };
    }

    const linePoints = [...points];
    const hasOriginPoint = linePoints.some((point) => point.x === 0);
    if (!hasOriginPoint) {
      linePoints.push({ index: -1, x: 0, y: 0, isRenderable: false });
    }

    linePoints.sort((a, b) => a.x - b.x);

    const derivedXRange = linePoints.reduce((range, point) => ({
      min: Math.min(range.min, point.x),
      max: Math.max(range.max, point.x)
    }), { min: Number.POSITIVE_INFINITY, max: Number.NEGATIVE_INFINITY });
    const derivedYRange = linePoints.reduce((range, point) => ({
      min: Math.min(range.min, point.y),
      max: Math.max(range.max, point.y)
    }), { min: Number.POSITIVE_INFINITY, max: Number.NEGATIVE_INFINITY });
    const safeMinX = Number.isFinite(axisMax.minX) ? axisMax.minX : (Number.isFinite(derivedXRange.min) ? derivedXRange.min : 0);
    const safeMaxX = Number.isFinite(axisMax.maxX) ? axisMax.maxX : (Number.isFinite(derivedXRange.max) ? derivedXRange.max : 1);
    const safeMinY = Number.isFinite(axisMax.minY) ? axisMax.minY : (Number.isFinite(derivedYRange.min) ? derivedYRange.min : 0);
    const safeMaxY = Number.isFinite(axisMax.maxY) ? axisMax.maxY : (Number.isFinite(derivedYRange.max) ? derivedYRange.max : 1);
    const bounds = {
      minX: safeMinX,
      maxX: safeMaxX,
      minY: safeMinY,
      maxY: safeMaxY
    };
    const path = linePoints
      .map((point, index) => projectChartPoint(point, bounds))
      .map((point, index) => {
        return `${index === 0 ? "M" : "L"}${point.px.toFixed(2)},${point.py.toFixed(2)}`;
      })
      .join(" ");
    const markerPoints = points
      .filter((point) => point.isRenderable)
      .sort((a, b) => a.x - b.x)
      .map((point) => projectChartPoint(point, bounds));

    return {
      path,
      points: markerPoints
    };
  };

  const buildMarkerMarkup = (points, seriesMeta, xKey) => {
    const xMeta = chartXAxisMeta[xKey] || chartXAxisMeta.throttle_percent;
    return points
      .map((point, index) => {
        const xLabel = formatTooltipValue(point.x, xMeta.unit, xMeta.digits);
        const yLabel = formatTooltipValue(point.y, seriesMeta.unit, seriesMeta.digits);
        return `<span class="chart-marker" data-tooltip-x="${escapeHtml(xLabel)}" data-tooltip-y="${escapeHtml(yLabel)}" data-tooltip-color="${escapeHtml(seriesMeta.color)}" style="left:${point.px.toFixed(2)}%;top:${point.py.toFixed(2)}%;--marker-color:${seriesMeta.color}" aria-hidden="true"></span>`;
      })
      .join(" ");
  };

  const buildMarkerOverlayMarkup = (markerMarkup = "") => `
    <div class="chart-marker-overlay">
      ${markerMarkup}
      <div class="chart-tooltip" hidden>
        <div class="chart-tooltip__line chart-tooltip__line--x"></div>
        <div class="chart-tooltip__line chart-tooltip__line--y"></div>
      </div>
    </div>
  `;

  const renderLayerMarkers = (layerElement, points, seriesMeta, xKey) => {
    if (!layerElement) {
      return;
    }

    layerElement.querySelector(".chart-marker-overlay")?.remove();
    layerElement.insertAdjacentHTML("beforeend", buildMarkerOverlayMarkup(buildMarkerMarkup(points, seriesMeta, xKey)));
  };

  const hideChartTooltip = (overlay) => {
    const tooltip = overlay?.querySelector(".chart-tooltip");
    if (!tooltip) {
      return;
    }

    tooltip.hidden = true;
    tooltip.classList.remove("chart-tooltip--visible");
  };

  const positionChartTooltip = (marker) => {
    const overlay = marker.closest(".chart-marker-overlay");
    const tooltip = overlay?.querySelector(".chart-tooltip");
    if (!overlay || !tooltip) {
      return;
    }

    const xLine = tooltip.querySelector(".chart-tooltip__line--x");
    const yLine = tooltip.querySelector(".chart-tooltip__line--y");
    if (xLine) {
      xLine.textContent = `X: ${marker.dataset.tooltipX || "--"}`;
    }
    if (yLine) {
      yLine.textContent = `Y: ${marker.dataset.tooltipY || "--"}`;
    }

    tooltip.style.setProperty("--tooltip-accent", marker.dataset.tooltipColor || "#244d74");
    tooltip.hidden = false;
    tooltip.classList.add("chart-tooltip--visible");

    const markerCenterX = marker.offsetLeft + (marker.offsetWidth / 2);
    const markerCenterY = marker.offsetTop + (marker.offsetHeight / 2);
    let left = markerCenterX + 8;
    let top = markerCenterY - tooltip.offsetHeight - 8;

    if (left + tooltip.offsetWidth > overlay.clientWidth - 4) {
      left = markerCenterX - tooltip.offsetWidth - 8;
    }
    if (left < 4) {
      left = 4;
    }
    if (top < 4) {
      top = markerCenterY + 8;
    }
    if (top + tooltip.offsetHeight > overlay.clientHeight - 4) {
      top = Math.max(4, overlay.clientHeight - tooltip.offsetHeight - 4);
    }

    tooltip.style.left = `${left}px`;
    tooltip.style.top = `${top}px`;
  };

  const hideAllChartTooltips = (plotElement) => {
    plotElement?.querySelectorAll(".chart-marker-overlay").forEach((overlay) => {
      hideChartTooltip(overlay);
    });
  };

  const initializeChartMarkerInteractions = (plotElement) => {
    if (!plotElement || plotElement.dataset.chartMarkerInteractions === "ready") {
      return;
    }

    plotElement.dataset.chartMarkerInteractions = "ready";
    plotElement.addEventListener("mouseover", (event) => {
      const marker = event.target.closest(".chart-marker");
      if (!marker || !plotElement.contains(marker)) {
        return;
      }

      positionChartTooltip(marker);
    });

    plotElement.addEventListener("mousemove", (event) => {
      const marker = event.target.closest(".chart-marker");
      if (!marker || !plotElement.contains(marker)) {
        return;
      }

      positionChartTooltip(marker);
    });

    plotElement.addEventListener("mouseout", (event) => {
      const marker = event.target.closest(".chart-marker");
      if (!marker || !plotElement.contains(marker)) {
        return;
      }

      hideChartTooltip(marker.closest(".chart-marker-overlay"));
    });

    plotElement.addEventListener("mouseleave", () => {
      hideAllChartTooltips(plotElement);
    });
  };

  const getChartAxisBounds = (rows, xAxisKey) => {
    const xRange = xAxisKey === "thrust_grams"
      ? getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 })
      : { min: 0, max: 100 };
    const thrustRange = getSeriesRange(rows, "thrust_grams", { fallbackMin: 0, fallbackMax: 1 });
    const efficiencyRange = getSeriesRange(rows, "thrust_efficiency_g_per_w", { fallbackMin: 0, fallbackMax: 1 });
    const voltageRange = getSeriesRange(rows, "voltage_v", { fallbackMin: 0, fallbackMax: 1 });
    const powerRange = getSeriesRange(rows, "power_w", { fallbackMin: 0, fallbackMax: 1 });
    const currentRange = getSeriesRange(rows, "current_a", { fallbackMin: 0, fallbackMax: 1 });
    const rpmRange = getSeriesRange(rows, "rpm", { fallbackMin: 0, fallbackMax: 1 });
    const tempRange = getSeriesRange(rows, "motor_temperature_c", { fallbackMin: 0, fallbackMax: 1 });

    return {
      x: xRange,
      thrust: thrustRange,
      efficiency: efficiencyRange,
      voltage: voltageRange,
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
      const geometry = buildChartGeometry(rows, chartSeriesMeta.thrust.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.thrust.min,
        maxY: axisBounds.thrust.max
      });
      context.thrustPath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.thrustLayer, geometry.points, chartSeriesMeta.thrust, xAxisKey);
      context.thrustPath.parentElement.style.display = hideThrustSeries || visibleSeries.thrust === false ? "none" : "";
    }

    if (context.efficiencyPath) {
      const geometry = buildChartGeometry(rows, chartSeriesMeta.efficiency.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.efficiency.min,
        maxY: axisBounds.efficiency.max
      });
      context.efficiencyPath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.efficiencyLayer, geometry.points, chartSeriesMeta.efficiency, xAxisKey);
      context.efficiencyPath.parentElement.style.display = visibleSeries.efficiency === false ? "none" : "";
    }

    if (context.voltagePath) {
      const geometry = buildChartGeometry(rows, chartSeriesMeta.voltage.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.voltage.min,
        maxY: axisBounds.voltage.max
      });
      context.voltagePath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.voltageLayer, geometry.points, chartSeriesMeta.voltage, xAxisKey);
      context.voltagePath.parentElement.style.display = visibleSeries.voltage === false ? "none" : "";
    }

    if (context.powerPath) {
      const geometry = buildChartGeometry(rows, chartSeriesMeta.power.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.power.min,
        maxY: axisBounds.power.max
      });
      context.powerPath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.powerLayer, geometry.points, chartSeriesMeta.power, xAxisKey);
      context.powerPath.parentElement.style.display = visibleSeries.power === false ? "none" : "";
    }

    if (context.currentPath) {
      const geometry = buildChartGeometry(rows, chartSeriesMeta.current.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.current.min,
        maxY: axisBounds.current.max
      });
      context.currentPath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.currentLayer, geometry.points, chartSeriesMeta.current, xAxisKey);
      context.currentPath.parentElement.style.display = visibleSeries.current === false ? "none" : "";
    }

    if (context.rpmPath) {
      const geometry = buildChartGeometry(rows, chartSeriesMeta.rpm.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.rpm.min,
        maxY: axisBounds.rpm.max
      });
      context.rpmPath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.rpmLayer, geometry.points, chartSeriesMeta.rpm, xAxisKey);
      context.rpmPath.parentElement.style.display = visibleSeries.rpm === false ? "none" : "";
    }

    if (context.tempPath) {
      const geometry = buildChartGeometry(rows, chartSeriesMeta.temp.valueKey, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.temp.min,
        maxY: axisBounds.temp.max
      });
      context.tempPath.setAttribute("d", geometry.path);
      renderLayerMarkers(context.tempLayer, geometry.points, chartSeriesMeta.temp, xAxisKey);
      context.tempPath.parentElement.style.display = visibleSeries.temp === false ? "none" : "";
    }
  };

  const buildStudyLayerMarkup = (datasets, seriesMeta, xKey, axisMax) => {
    const markers = [];
    const lines = datasets.map((dataset) => {
      const hidden = dataset.visible === false ? " style=\"display:none\"" : "";
      const geometry = buildChartGeometry(dataset.rows, seriesMeta.valueKey, xKey, axisMax);
      if (dataset.visible !== false) {
        markers.push(buildMarkerMarkup(geometry.points, seriesMeta, xKey));
      }

      return `<svg class="chart-study-line ${seriesMeta.className}" viewBox="0 0 100 100" preserveAspectRatio="none" data-study-dataset-id="${escapeHtml(dataset.id)}"${hidden}><path d="${escapeHtml(geometry.path)}"></path></svg>`;
    }).join("");

    return `${lines}${buildMarkerOverlayMarkup(markers.join(""))}`;
  };

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
      .map((dataset) => `
          <label class="chart-file-picker__option" for="study-dataset-${escapeHtml(dataset.id)}">
            <input id="study-dataset-${escapeHtml(dataset.id)}" type="checkbox" name="study-dataset-visibility" value="${escapeHtml(dataset.id)}"${dataset.visible === false ? "" : " checked"}>
            <span>
              <span class="chart-file-picker__name">${escapeHtml(dataset.name)}</span>
            </span>
          </label>
        `)
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
      ui.studyEfficiencySummaryGroup?.style.setProperty("--study-file-picker-max-height", `${Math.floor(maxHeight)}px`);
    } else {
      ui.studyFileVisibilityGroup.style.removeProperty("--study-file-picker-max-height");
      ui.studyEfficiencySummaryGroup?.style.removeProperty("--study-file-picker-max-height");
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
      context.thrustLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.thrust, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.thrust.min,
        maxY: axisBounds.thrust.max
      });
      context.thrustLayer.style.display = hideThrustSeries || visibleSeries.thrust === false ? "none" : "";
    }

    if (context.efficiencyLayer) {
      context.efficiencyLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.efficiency, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.efficiency.min,
        maxY: axisBounds.efficiency.max
      });
      context.efficiencyLayer.style.display = visibleSeries.efficiency === false ? "none" : "";
    }

    if (context.voltageLayer) {
      context.voltageLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.voltage, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.voltage.min,
        maxY: axisBounds.voltage.max
      });
      context.voltageLayer.style.display = visibleSeries.voltage === false ? "none" : "";
    }

    if (context.powerLayer) {
      context.powerLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.power, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.power.min,
        maxY: axisBounds.power.max
      });
      context.powerLayer.style.display = visibleSeries.power === false ? "none" : "";
    }

    if (context.currentLayer) {
      context.currentLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.current, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.current.min,
        maxY: axisBounds.current.max
      });
      context.currentLayer.style.display = visibleSeries.current === false ? "none" : "";
    }

    if (context.rpmLayer) {
      context.rpmLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.rpm, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.rpm.min,
        maxY: axisBounds.rpm.max
      });
      context.rpmLayer.style.display = visibleSeries.rpm === false ? "none" : "";
    }

    if (context.tempLayer) {
      context.tempLayer.innerHTML = buildStudyLayerMarkup(studyDatasets, chartSeriesMeta.temp, xAxisKey, {
        minX: axisBounds.x.min,
        maxX: axisBounds.x.max,
        minY: axisBounds.temp.min,
        maxY: axisBounds.temp.max
      });
      context.tempLayer.style.display = visibleSeries.temp === false ? "none" : "";
    }

    renderStudyFileControls();
    syncStudyFilePickerHeight();
    updateStudyEfficiencySummary(studyDatasets);
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

    return normalizeChartRows(lines.slice(1)
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
      }));
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

  const setConfigurationControlsState = (disabled = false) => {
    [
      ui.configEscPolesValue,
      ui.configEscPolesButton,
      ui.configEscReverseButton,
      ui.configCurrentLowValue,
      ui.configCurrentLowButton,
      ui.configCurrentHighValue,
      ui.configCurrentHighButton,
      ui.configVoltageLowValue,
      ui.configVoltageLowButton,
      ui.configVoltageHighValue,
      ui.configVoltageHighButton,
      ui.configScaleTareButton,
      ui.configScaleCalibrationValue,
      ui.configScaleCalibrationButton,
      ui.configSafetyCurrentHiValue,
      ui.configSafetyCurrentHiButton,
      ui.configSafetyCurrentHiHiValue,
      ui.configSafetyCurrentHiHiButton,
      ui.configSafetyMotorTempHiValue,
      ui.configSafetyMotorTempHiButton,
      ui.configSafetyMotorTempHiHiValue,
      ui.configSafetyMotorTempHiHiButton,
      ui.configSafetyEscTempHiValue,
      ui.configSafetyEscTempHiButton,
      ui.configSafetyEscTempHiHiValue,
      ui.configSafetyEscTempHiHiButton
    ].forEach((element) => {
      if (element) {
        element.disabled = disabled;
      }
    });
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
    if (ui.configScaleFactorValue) {
      ui.configScaleFactorValue.value = "--";
    }
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

    if (ui.overviewMotorControlCard) {
      ui.overviewMotorControlCard.hidden = view === "testing";
    }

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
      cachedTestResults = normalizeChartRows(data.test_results);
      cachedTestResultCount = resultCount;
    }

    setText(ui.voltage, formatNumber(data.voltage_v, "V", 3));
    setText(ui.current, formatAdaptiveNumber(data.current_a, "A"));
    setText(ui.power, formatAdaptiveNumber(data.power_w, "W"));
    setText(ui.overviewThrottle, formatNumber(data.throttle_percent, "%", 1));
    setText(ui.testingThrottle, formatNumber(data.throttle_percent, "%", 1));
    setText(ui.rpm, formatInteger(data.rpm, "rpm"));
    setText(ui.escTemp, formatNumber(data.esc_temperature_c, "deg C", 1));
    setText(ui.motorTemp, formatNumber(data.ir_object_c, "deg C", 1));
    setText(ui.ambientTemp, formatNumber(data.ir_ambient_c, "deg C", 1));
    setText(ui.thrust, formatAdaptiveNumber(data.thrust_grams, "g"));
    setText(ui.thrustStdDev, `Std dev ${formatNumber(data.thrust_stddev_grams, "g", 2)}`);
    setFixedInputValueIfIdle(ui.configEscPolesValue, Number(data.motor_poles) || 0, 0);
    if (ui.configScaleFactorValue) {
      ui.configScaleFactorValue.value = Number.isFinite(Number(data.scale_calibration_factor))
        ? Number(data.scale_calibration_factor).toFixed(6)
        : "--";
    }
    setThresholdInputValueIfIdle(ui.configSafetyCurrentHiValue, data.safety_current_hi_a, 0);
    setThresholdInputValueIfIdle(ui.configSafetyCurrentHiHiValue, data.safety_current_hihi_a, 0);
    setThresholdInputValueIfIdle(ui.configSafetyMotorTempHiValue, data.safety_motor_temp_hi_c, 0);
    setThresholdInputValueIfIdle(ui.configSafetyMotorTempHiHiValue, data.safety_motor_temp_hihi_c, 0);
    setThresholdInputValueIfIdle(ui.configSafetyEscTempHiValue, data.safety_esc_temp_hi_c, 0);
    setThresholdInputValueIfIdle(ui.configSafetyEscTempHiHiValue, data.safety_esc_temp_hihi_c, 0);
    updateChart(chartContexts.testing, cachedTestResults);
    updateTestingEfficiencySummary(cachedTestResults, Boolean(data.test_running));

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
      setHealth(ui.powerHealth, "Computed from live feed");
      setHealth(ui.rpmHealth, "RPM telemetry healthy");
    } else {
      setHealth(ui.overviewThrottleHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.testingThrottleHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.voltageHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.currentHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.powerHealth, "Waiting for live feed", "warn");
      setHealth(ui.rpmHealth, "Awaiting ESC telemetry", "warn");
      setHealth(ui.escTempHealth, "Awaiting ESC telemetry", "warn");
    }

    if (hasTelemetry) {
      const currentSafety = describeSafetyState(
        data.safety_current_state,
        "Current sensor healthy",
        "Current HI warning active",
        "Current HIHI stop triggered",
        "Awaiting ESC telemetry"
      );
      setHealth(ui.currentHealth, currentSafety.text, currentSafety.severity);

      const escTempSafety = describeSafetyState(
        data.safety_esc_temp_state,
        "ESC thermal sensor healthy",
        "ESC temp HI warning active",
        "ESC temp HIHI stop triggered",
        "Awaiting ESC telemetry"
      );
      setHealth(ui.escTempHealth, escTempSafety.text, escTempSafety.severity);
    }

    if (scaleReady) {
      setHealth(ui.thrustHealth, "Load cell healthy");
    } else if (hasScale) {
      setHealth(ui.thrustHealth, "Scale stabilizing", "warn");
    } else {
      setHealth(ui.thrustHealth, "Scale offline", "error");
    }

    if (hasIr) {
      const motorTempSafety = describeSafetyState(
        data.safety_motor_temp_state,
        "IR target healthy",
        "Motor temp HI warning active",
        "Motor temp HIHI stop triggered",
        "IR sensor offline"
      );
      setHealth(ui.motorTempHealth, motorTempSafety.text, motorTempSafety.severity);
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
    pollTimer = window.setInterval(pollStatus, 250);
  };

  const setOverviewMotorControlsState = (disabled) => {
    if (ui.overviewMotorSetValue) {
      ui.overviewMotorSetValue.disabled = disabled;
    }
    if (ui.overviewMotorSetButton) {
      ui.overviewMotorSetButton.disabled = disabled;
    }
    if (ui.overviewMotorStopButton) {
      ui.overviewMotorStopButton.disabled = disabled;
    }
  };

  const sendOverviewMotorCommand = async (command, pendingText) => {
    if (overviewMotorCommandPending) {
      return;
    }

    overviewMotorCommandPending = true;
    setOverviewMotorControlsState(true);
    if (pendingText) {
      setHealth(ui.overviewThrottleHealth, pendingText, "warn");
    }

    try {
      const body = new URLSearchParams({ cmd: command });
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
      setHealth(ui.overviewThrottleHealth, "Motor command failed", "error");
    } finally {
      overviewMotorCommandPending = false;
      setOverviewMotorControlsState(false);
    }
  };

  const sendConfigurationCommand = async (command) => {
    if (configurationCommandPending) {
      return false;
    }

    configurationCommandPending = true;
    setConfigurationControlsState(true);

    try {
      const body = new URLSearchParams({ cmd: command });
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
      return true;
    } catch (error) {
      return false;
    } finally {
      configurationCommandPending = false;
      setConfigurationControlsState(false);
    }
  };

  const runOverviewMotorSet = async () => {
    if (!ui.overviewMotorSetValue) {
      return;
    }

    const targetValue = Number(ui.overviewMotorSetValue.value);
    if (!Number.isFinite(targetValue) || targetValue < 0 || targetValue > 100) {
      setHealth(ui.overviewThrottleHealth, "Enter 0 to 100%", "error");
      ui.overviewMotorSetValue.focus();
      return;
    }

    const normalizedValue = Number(targetValue.toFixed(1));
    ui.overviewMotorSetValue.value = `${normalizedValue}`;
    await sendOverviewMotorCommand(`motor set ${normalizedValue}`, `Setting ${normalizedValue.toFixed(1)}%`);
  };

  const runOverviewMotorStop = async () => {
    await sendOverviewMotorCommand("motor stop", "Stopping motor");
  };

  const runEscPolesCommand = async () => {
    if (!ui.configEscPolesValue) {
      return;
    }

    const targetValue = Number(ui.configEscPolesValue.value);
    if (!Number.isInteger(targetValue) || targetValue < 2 || targetValue > 100) {
      ui.configEscPolesValue.focus();
      return;
    }

    ui.configEscPolesValue.value = `${targetValue}`;
    await sendConfigurationCommand(`esc poles ${targetValue}`);
  };

  const runEscReverseCommand = async () => {
    await sendConfigurationCommand("esc reverse");
  };

  const runCalibrationPointCommand = async (input, prefix) => {
    if (!input) {
      return;
    }

    const targetValue = Number(input.value);
    if (!Number.isFinite(targetValue) || targetValue < 0) {
      input.focus();
      return;
    }

    const normalizedValue = Number(targetValue.toFixed(3));
    input.value = `${normalizedValue}`;
    await sendConfigurationCommand(`${prefix}${normalizedValue}`);
  };

  const runCurrentLowCalibrationCommand = async () => {
    await runCalibrationPointCommand(ui.configCurrentLowValue, "calibrate current low ");
  };

  const runCurrentHighCalibrationCommand = async () => {
    await runCalibrationPointCommand(ui.configCurrentHighValue, "calibrate current high ");
  };

  const runVoltageLowCalibrationCommand = async () => {
    await runCalibrationPointCommand(ui.configVoltageLowValue, "calibrate voltage low ");
  };

  const runVoltageHighCalibrationCommand = async () => {
    await runCalibrationPointCommand(ui.configVoltageHighValue, "calibrate voltage high ");
  };

  const runScaleTareCommand = async () => {
    await sendConfigurationCommand("scale tare");
  };

  const runScaleCalibrationCommand = async () => {
    if (!ui.configScaleCalibrationValue) {
      return;
    }

    const targetValue = Number(ui.configScaleCalibrationValue.value);
    if (!Number.isFinite(targetValue) || targetValue <= 0) {
      ui.configScaleCalibrationValue.focus();
      return;
    }

    const normalizedValue = Number(targetValue.toFixed(3));
    ui.configScaleCalibrationValue.value = `${normalizedValue}`;
    await sendConfigurationCommand(`scale calibration ${normalizedValue}`);
  };

  const runSafetyThresholdCommand = async (input, prefix, digits = 3) => {
    if (!input) {
      return;
    }

    if (`${input.value}`.trim() === "") {
      input.focus();
      return;
    }

    const targetValue = Number(input.value);
    if (!Number.isFinite(targetValue) || targetValue < 0) {
      input.focus();
      return;
    }

    const normalizedValue = Number(targetValue.toFixed(digits));
    input.value = `${normalizedValue}`;
    await sendConfigurationCommand(`${prefix}${normalizedValue}`);
  };

  const runSafetyCurrentHiCommand = async () => runSafetyThresholdCommand(ui.configSafetyCurrentHiValue, "set current hi ", 0);
  const runSafetyCurrentHiHiCommand = async () => runSafetyThresholdCommand(ui.configSafetyCurrentHiHiValue, "set current hihi ", 0);
  const runSafetyMotorTempHiCommand = async () => runSafetyThresholdCommand(ui.configSafetyMotorTempHiValue, "set motor temp hi ", 0);
  const runSafetyMotorTempHiHiCommand = async () => runSafetyThresholdCommand(ui.configSafetyMotorTempHiHiValue, "set motor temp hihi ", 0);
  const runSafetyEscTempHiCommand = async () => runSafetyThresholdCommand(ui.configSafetyEscTempHiValue, "set esc temp hi ", 0);
  const runSafetyEscTempHiHiCommand = async () => runSafetyThresholdCommand(ui.configSafetyEscTempHiHiValue, "set esc temp hihi ", 0);

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
    updateTestingEfficiencySummary(cachedTestResults, true);

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

  if (ui.overviewMotorSetButton) {
    ui.overviewMotorSetButton.addEventListener("click", runOverviewMotorSet);
  }

  if (ui.overviewMotorSetValue) {
    ui.overviewMotorSetValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runOverviewMotorSet();
      }
    });
  }

  if (ui.overviewMotorStopButton) {
    ui.overviewMotorStopButton.addEventListener("click", runOverviewMotorStop);
  }

  if (ui.configEscPolesButton) {
    ui.configEscPolesButton.addEventListener("click", runEscPolesCommand);
  }

  if (ui.configEscPolesValue) {
    ui.configEscPolesValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runEscPolesCommand();
      }
    });
  }

  if (ui.configEscReverseButton) {
    ui.configEscReverseButton.addEventListener("click", runEscReverseCommand);
  }

  if (ui.configCurrentLowButton) {
    ui.configCurrentLowButton.addEventListener("click", runCurrentLowCalibrationCommand);
  }

  if (ui.configCurrentLowValue) {
    ui.configCurrentLowValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runCurrentLowCalibrationCommand();
      }
    });
  }

  if (ui.configCurrentHighButton) {
    ui.configCurrentHighButton.addEventListener("click", runCurrentHighCalibrationCommand);
  }

  if (ui.configCurrentHighValue) {
    ui.configCurrentHighValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runCurrentHighCalibrationCommand();
      }
    });
  }

  if (ui.configVoltageLowButton) {
    ui.configVoltageLowButton.addEventListener("click", runVoltageLowCalibrationCommand);
  }

  if (ui.configVoltageLowValue) {
    ui.configVoltageLowValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runVoltageLowCalibrationCommand();
      }
    });
  }

  if (ui.configVoltageHighButton) {
    ui.configVoltageHighButton.addEventListener("click", runVoltageHighCalibrationCommand);
  }

  if (ui.configVoltageHighValue) {
    ui.configVoltageHighValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runVoltageHighCalibrationCommand();
      }
    });
  }

  if (ui.configScaleTareButton) {
    ui.configScaleTareButton.addEventListener("click", runScaleTareCommand);
  }

  if (ui.configScaleCalibrationButton) {
    ui.configScaleCalibrationButton.addEventListener("click", runScaleCalibrationCommand);
  }

  if (ui.configScaleCalibrationValue) {
    ui.configScaleCalibrationValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runScaleCalibrationCommand();
      }
    });
  }

  if (ui.configSafetyCurrentHiButton) {
    ui.configSafetyCurrentHiButton.addEventListener("click", runSafetyCurrentHiCommand);
  }

  if (ui.configSafetyCurrentHiValue) {
    ui.configSafetyCurrentHiValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runSafetyCurrentHiCommand();
      }
    });
  }

  if (ui.configSafetyCurrentHiHiButton) {
    ui.configSafetyCurrentHiHiButton.addEventListener("click", runSafetyCurrentHiHiCommand);
  }

  if (ui.configSafetyCurrentHiHiValue) {
    ui.configSafetyCurrentHiHiValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runSafetyCurrentHiHiCommand();
      }
    });
  }

  if (ui.configSafetyMotorTempHiButton) {
    ui.configSafetyMotorTempHiButton.addEventListener("click", runSafetyMotorTempHiCommand);
  }

  if (ui.configSafetyMotorTempHiValue) {
    ui.configSafetyMotorTempHiValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runSafetyMotorTempHiCommand();
      }
    });
  }

  if (ui.configSafetyMotorTempHiHiButton) {
    ui.configSafetyMotorTempHiHiButton.addEventListener("click", runSafetyMotorTempHiHiCommand);
  }

  if (ui.configSafetyMotorTempHiHiValue) {
    ui.configSafetyMotorTempHiHiValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runSafetyMotorTempHiHiCommand();
      }
    });
  }

  if (ui.configSafetyEscTempHiButton) {
    ui.configSafetyEscTempHiButton.addEventListener("click", runSafetyEscTempHiCommand);
  }

  if (ui.configSafetyEscTempHiValue) {
    ui.configSafetyEscTempHiValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runSafetyEscTempHiCommand();
      }
    });
  }

  if (ui.configSafetyEscTempHiHiButton) {
    ui.configSafetyEscTempHiHiButton.addEventListener("click", runSafetyEscTempHiHiCommand);
  }

  if (ui.configSafetyEscTempHiHiValue) {
    ui.configSafetyEscTempHiHiValue.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runSafetyEscTempHiHiCommand();
      }
    });
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

  initializeChartMarkerInteractions(chartContexts.testing.plot);
  initializeChartMarkerInteractions(chartContexts.study.plot);
});


