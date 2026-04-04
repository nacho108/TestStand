window.addEventListener("load", () => {
  document.body.classList.add("ready");

  const hostingStatus = document.getElementById("hosting-status");
  const apiStatus = document.getElementById("api-status");
  const voltageValue = document.getElementById("voltage-value");
  const currentValue = document.getElementById("current-value");
  const rpmValue = document.getElementById("rpm-value");

  if (hostingStatus) {
    hostingStatus.textContent = "Static page loaded";
  }

  const formatValue = (value, unit, digits = 2) => {
    if (value === null || value === undefined) {
      return "--";
    }

    return `${Number(value).toFixed(digits)} ${unit}`;
  };

  const refreshStatus = async () => {
    try {
      const response = await fetch("/api/status", { cache: "no-store" });
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const data = await response.json();

      if (apiStatus) {
        apiStatus.textContent = data.telemetry_valid ? "Live data OK" : "Waiting for telemetry";
      }

      if (voltageValue) {
        voltageValue.textContent = formatValue(data.voltage_v, "V", 3);
      }

      if (currentValue) {
        currentValue.textContent = formatValue(data.current_a, "A", 3);
      }

      if (rpmValue) {
        rpmValue.textContent = data.rpm === null ? "--" : `${Math.round(data.rpm)} rpm`;
      }
    } catch (error) {
      if (apiStatus) {
        apiStatus.textContent = "Request failed";
      }
    }
  };

  refreshStatus();
  window.setInterval(refreshStatus, 1000);
});
