window.addEventListener("load", () => {
  document.body.classList.add("ready");

  const hostingStatus = document.getElementById("hosting-status");
  if (hostingStatus) {
    hostingStatus.textContent = "Static page loaded";
  }
});
