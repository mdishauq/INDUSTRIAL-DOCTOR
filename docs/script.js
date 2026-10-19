/**
 * Initialize AOS (Animate On Scroll) if available
 */
function initAOS() {
  if (typeof AOS !== 'undefined') {
    AOS.init({ duration: 800, once: true });
  }
}

/**
 * Initialize code syntax highlighting with Highlight.js
 */
function initCodeHighlighting() {
  if (typeof hljs !== 'undefined') {
    document.querySelectorAll('pre code').forEach((block) => {
      hljs.highlightElement(block);
    });
  }
}

// Telemetry simulation loop data
const telemetryLoop = [
  { state: "HEALTHY", color: "#2d7a68", confidence: 91, anomaly: "0.12", note: "Motor RMS stable, anomaly Z-score within normal bounds." },
  { state: "HEALTHY", color: "#2d7a68", confidence: 94, anomaly: "0.09", note: "Consistent operational envelope on Core 1." },
  { state: "AMBIENT", color: "#64748b", confidence: 52, anomaly: "0.04", note: "RMS below ambient threshold guardrail." },
  { state: "HEALTHY", color: "#2d7a68", confidence: 89, anomaly: "0.15", note: "Motor restarted successfully." },
  { state: "BEARING FAULT", color: "#d97706", confidence: 78, anomaly: "0.72", note: "Crest factor > 4.1 detected impact spike." },
];

/**
 * Initialize hero state telemetry loop animation with null checks
 */
function initHeroTelemetryAnimation() {
  const stateEl = document.getElementById("hero-state");
  const noteEl = document.getElementById("hero-note");
  const fillEl = document.getElementById("meter-fill");
  const valueEl = document.getElementById("meter-value");
  const anomalyEl = document.getElementById("anomaly-score-disp");

  if (!stateEl || !noteEl || !fillEl || !valueEl || !anomalyEl) return;

  let i = 0;
  const paint = () => {
    const item = telemetryLoop[i % telemetryLoop.length];
    stateEl.style.opacity = "0.5";
    setTimeout(() => {
      stateEl.textContent = item.state;
      stateEl.style.color = item.color;
      noteEl.textContent = item.note;
      fillEl.style.width = `${item.confidence}%`;
      valueEl.textContent = `${item.confidence}%`;
      anomalyEl.textContent = item.anomaly;
      stateEl.style.opacity = "1";
    }, 150);
    i++;
  };

  paint();
  window.setInterval(paint, 3200);
}

/**
 * Reveal elements as they scroll into view using Intersection Observer
 */
function initRevealAnimations() {
  const revealItems = document.querySelectorAll(".reveal");
  if (!revealItems.length) return;

  const obs = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add("visible");
          obs.unobserve(entry.target);
        }
      });
    },
    { threshold: 0.1 }
  );

  revealItems.forEach((el) => obs.observe(el));
}

/**
 * Initialize Chart.js Analytics charts safely
 */
function initCharts() {
  if (typeof Chart === "undefined") return;

  const textColor = "#2d5a4f";
  const gridColor = "rgba(45, 122, 104, 0.08)";

  // Status Distribution Doughnut Chart
  const statusCanvas = document.getElementById("statusChart");
  if (statusCanvas) {
    new Chart(statusCanvas, {
      type: "doughnut",
      data: {
        labels: ["Healthy", "Ambient", "Bearing/Friction Fault"],
        datasets: [{
          data: [70, 20, 10],
          backgroundColor: ["#2d7a68", "#64748b", "#d97706"],
          borderWidth: 2,
          borderColor: "#ffffff"
        }]
      },
      options: {
        responsive: true,
        plugins: {
          legend: { position: "bottom", labels: { color: textColor, font: { family: "Sora", size: 11 } } }
        }
      }
    });
  }

  // Trend Line Chart
  const trendCanvas = document.getElementById("trendChart");
  if (trendCanvas) {
    new Chart(trendCanvas, {
      type: "line",
      data: {
        labels: ["0s", "10s", "20s", "30s", "40s", "50s", "60s"],
        datasets: [{
          label: "Z-Score Anomaly Probability",
          data: [0.12, 0.10, 0.14, 0.11, 0.13, 0.68, 0.72],
          borderColor: "#00d9ff",
          backgroundColor: "rgba(0, 217, 255, 0.1)",
          fill: true,
          tension: 0.3,
          borderWidth: 2
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: { ticks: { color: textColor, font: { family: "Space Mono", size: 10 } }, grid: { color: gridColor } },
          y: { ticks: { color: textColor, font: { family: "Space Mono", size: 10 } }, grid: { color: gridColor } }
        },
        plugins: {
          legend: { labels: { color: textColor, font: { family: "Sora", size: 11 } } }
        }
      }
    });
  }
}

/**
 * OLED canvas animations and feature screen simulation
 */
function initOledSimulations() {
  const hbCanvas = document.getElementById("heartbeat");
  if (hbCanvas) {
    const ctx = hbCanvas.getContext("2d");
    const heartbeat = [0, 0, 0, 0, -3, -6, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let phase = 0;
    setInterval(() => {
      ctx.clearRect(0, 0, hbCanvas.width, hbCanvas.height);
      ctx.strokeStyle = "#00d9ff";
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      for (let i = 0; i < 120; i++) {
        const idx = (Math.floor(i / 4) + phase) % 16;
        const y = 8 + heartbeat[idx] * 0.8;
        i === 0 ? ctx.moveTo(i, y) : ctx.lineTo(i, y);
      }
      ctx.stroke();
      phase = (phase + 1) % 16;
    }, 60);
  }

  // Feature readout page rotation
  const features = [
    { label: "RMS", val: "245.8", label2: "PEAK HZ", val2: "1247 Hz" },
    { label: "KURTOSIS", val: "2.14", label2: "CREST", val2: "3.12" },
    { label: "CENTROID", val: "847 Hz", label2: "ZCR", val2: "0.168" },
  ];
  let fIdx = 0;
  setInterval(() => {
    const feat = features[fIdx % features.length];
    const pageNum = document.getElementById("page-num");
    const rmsVal = document.getElementById("rms-val");
    const hzVal = document.getElementById("hz-val");
    if (pageNum) pageNum.textContent = (fIdx % features.length) + 1;
    if (rmsVal) rmsVal.textContent = feat.val;
    if (hzVal) hzVal.textContent = feat.val2;
    fIdx++;
  }, 2500);
}

// Initialize everything securely on DOMContentLoaded
window.addEventListener("DOMContentLoaded", () => {
  requestAnimationFrame(() => {
    initHeroTelemetryAnimation();
    initRevealAnimations();
    initCharts();
    initOledSimulations();
    initCodeHighlighting();
    initAOS();
  });
});