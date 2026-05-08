/**
 * Initialize AOS (Animate On Scroll) animations
 * Provides scroll-triggered reveal animations
 */
function initAOS() {
  if (typeof AOS !== 'undefined') {
    AOS.init({
      duration: 800,
      easing: 'ease-in-out-cubic',
      once: false,
      mirror: false,
      offset: 100,
      delay: 0,
    });
  }
}

/**
 * Initialize code syntax highlighting
 * Highlights code blocks with Highlight.js
 */
function initCodeHighlighting() {
  if (typeof hljs !== 'undefined') {
    document.querySelectorAll('pre code').forEach((block) => {
      hljs.highlightElement(block);
    });
  }
}

// Enhanced state loop with more realistic transitions
const stateLoop = [
  { state: "HEALTHY", color: "#2d7a68", confidence: 87, note: "Machine behavior is stable and within expected operation." },
  { state: "HEALTHY", color: "#2d7a68", confidence: 91, note: "Condition remains steady with consistent operational response." },
  { state: "AMBIENT", color: "#64748b", confidence: 54, note: "No meaningful machine activity. Monitoring remains active." },
  { state: "HEALTHY", color: "#2d7a68", confidence: 89, note: "Machine has returned to normal profile and stable status." },
  { state: "FAULT", color: "#d97706", confidence: 78, note: "Noticeable deviation detected. Maintenance check recommended soon." },
];

/**
 * Initialize hero status animation
 * Cycles through different machine states with smooth transitions
 */
function initHeroStatusAnimation() {
  const stateEl = document.getElementById("hero-state");
  const noteEl = document.getElementById("hero-note");
  const fillEl = document.getElementById("meter-fill");
  const valueEl = document.getElementById("meter-value");

  if (!stateEl || !noteEl || !fillEl || !valueEl) return;

  let i = 0;

  const paint = () => {
    const item = stateLoop[i % stateLoop.length];
    
    // Animate state text with fade
    stateEl.style.opacity = "0.6";
    stateEl.style.transition = "opacity 300ms ease";
    
    setTimeout(() => {
      stateEl.textContent = item.state;
      stateEl.style.color = item.color;
      noteEl.textContent = item.note;
      fillEl.style.width = `${item.confidence}%`;
      valueEl.textContent = `${item.confidence}%`;
      stateEl.style.opacity = "1";
    }, 150);

    i += 1;
  };

  paint();
  window.setInterval(paint, 2800);
}

/**
 * Initialize reveal animations using Intersection Observer
 * Animates elements as they scroll into view
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
    { threshold: 0.15 }
  );

  revealItems.forEach((el) => obs.observe(el));
}

/**
 * Initialize Chart.js visualizations
 * Creates status distribution doughnut and trend line charts
 */
function initCharts() {
  if (typeof Chart === "undefined") return;

  const textColor = "#2d5a4f";
  const gridColor = "rgba(45, 122, 104, 0.08)";
  const primaryGreen = "#2d7a68";
  const lightGreen = "#4a9e89";

  // Status Distribution Doughnut Chart
  const statusCanvas = document.getElementById("statusChart");
  if (statusCanvas) {
    new Chart(statusCanvas, {
      type: "doughnut",
      data: {
        labels: ["Healthy", "Ambient", "Fault"],
        datasets: [
          {
            data: [66, 21, 13],
            backgroundColor: ["#2d7a68", "#64748b", "#d97706"],
            borderColor: ["#ffffff", "#ffffff", "#ffffff"],
            borderWidth: 3,
            hoverOffset: 8,
            borderRadius: 6,
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: true,
        plugins: {
          legend: {
            position: "bottom",
            labels: { 
              color: textColor, 
              font: { family: "Sora", size: 12, weight: "600" },
              padding: 15,
              boxHeight: 10,
            },
          },
          tooltip: {
            backgroundColor: "rgba(45, 122, 104, 0.9)",
            titleColor: "#fff",
            bodyColor: "#fff",
            borderColor: primaryGreen,
            borderWidth: 1,
            padding: 12,
            titleFont: { size: 13, weight: "bold" },
            callbacks: {
              label: (ctx) => ` ${ctx.label}: ${ctx.raw}%`,
            },
          },
        },
      },
    });
  }

  // Trend Line Chart
  const trendCanvas = document.getElementById("trendChart");
  if (trendCanvas) {
    new Chart(trendCanvas, {
      type: "line",
      data: {
        labels: ["08:00", "09:00", "10:00", "11:00", "12:00", "13:00", "14:00"],
        datasets: [
          {
            label: "Confidence Score",
            data: [84, 89, 86, 91, 88, 82, 87],
            borderColor: primaryGreen,
            backgroundColor: "rgba(45, 122, 104, 0.12)",
            fill: true,
            tension: 0.4,
            pointRadius: 5,
            pointHoverRadius: 7,
            pointBackgroundColor: primaryGreen,
            pointBorderColor: "#fff",
            pointBorderWidth: 2,
            borderWidth: 3,
            shadowColor: "rgba(45, 122, 104, 0.15)",
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: true,
        scales: {
          x: {
            ticks: { 
              color: textColor, 
              font: { family: "Space Mono", size: 10, weight: "600" },
              padding: 8,
            },
            grid: { 
              color: gridColor,
              drawBorder: false,
            },
          },
          y: {
            suggestedMin: 60,
            suggestedMax: 100,
            ticks: {
              callback: (v) => `${v}%`,
              color: textColor,
              font: { family: "Space Mono", size: 10, weight: "600" },
              padding: 8,
            },
            grid: { 
              color: gridColor,
              drawBorder: false,
            },
          },
        },
        plugins: {
          legend: {
            labels: { 
              color: textColor, 
              font: { family: "Sora", size: 12, weight: "600" },
              padding: 15,
              usePointStyle: true,
            },
          },
          tooltip: {
            backgroundColor: "rgba(45, 122, 104, 0.9)",
            titleColor: "#fff",
            bodyColor: "#fff",
            borderColor: primaryGreen,
            borderWidth: 1,
            padding: 12,
            titleFont: { size: 13, weight: "bold" },
          },
        },
      },
    });
  }
}

/**
 * Initialize smooth anchor scrolling
 * Allows smooth navigation between sections
 */
function initSmoothAnchors() {
  document.querySelectorAll('a[href^="#"]').forEach((a) => {
    a.addEventListener("click", (e) => {
      const id = a.getAttribute("href");
      const target = id ? document.querySelector(id) : null;
      if (!target) return;

      e.preventDefault();
      
      // Smooth scroll with offset for sticky header
      const headerOffset = 100;
      const elementPosition = target.getBoundingClientRect().top;
      const offsetPosition = elementPosition + window.pageYOffset - headerOffset;

      window.scrollTo({
        top: offsetPosition,
        behavior: "smooth"
      });

      // Add highlight effect
      setTimeout(() => {
        const originalBg = target.style.backgroundColor;
        target.style.backgroundColor = "rgba(0, 217, 255, 0.1)";
        target.style.transition = "background-color 600ms ease";
        setTimeout(() => {
          target.style.backgroundColor = originalBg;
        }, 600);
      }, 400);
    });
  });

  // Update active nav link on scroll
  window.addEventListener('scroll', () => {
    updateActiveNavLink();
  });
}

/**
 * Update active navigation link based on scroll position
 */
function updateActiveNavLink() {
  const sections = document.querySelectorAll('section[id]');
  const navLinks = document.querySelectorAll('nav a[href^="#"]');

  let current = '';
  sections.forEach((section) => {
    const sectionTop = section.offsetTop;
    const sectionHeight = section.clientHeight;
    if (pageYOffset >= sectionTop - 200) {
      current = section.getAttribute('id');
    }
  });

  navLinks.forEach((link) => {
    link.style.color = '';
    if (link.getAttribute('href') === '#' + current) {
      link.style.color = 'var(--green-primary)';
    }
  });
}

/**
 * Initialize OLED screen simulations
 * Creates animated visualizations of device displays
 */
function initOledSimulations() {
  // Heartbeat animation
  const hbCanvas = document.getElementById("heartbeat");
  if (hbCanvas) {
    const ctx = hbCanvas.getContext("2d");
    const heartbeat = [0, 0, 0, 0, -2, -4, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let phase = 0;
    
    setInterval(() => {
      ctx.clearRect(0, 0, hbCanvas.width, hbCanvas.height);
      ctx.strokeStyle = "#fff";
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      
      for (let i = 0; i < 120; i++) {
        const idx = (i / 4 + phase) % 16;
        const y = 8 + heartbeat[Math.floor(idx)] * 0.8;
        i === 0 ? ctx.moveTo(i, y) : ctx.lineTo(i, y);
      }
      
      ctx.stroke();
      phase = (phase + 1) % 16;
    }, 50);
  }

  // Sine wave animation (dual frequency)
  const sineCanvas = document.getElementById("sine-wave");
  if (sineCanvas) {
    const ctx = sineCanvas.getContext("2d");
    let offset = 0;
    
    setInterval(() => {
      ctx.clearRect(0, 0, sineCanvas.width, sineCanvas.height);
      ctx.strokeStyle = "#fff";
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      
      for (let x = 0; x < 120; x++) {
        const angle1 = (x + offset) * 0.12;
        const angle2 = (x + offset) * 0.07 + 1.2;
        const y1 = 9 + Math.sin(angle1) * 4;
        const y2 = 9 + Math.sin(angle2) * 2;
        const y = (y1 + y2) / 2;
        x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
      }
      
      ctx.stroke();
      offset = (offset + 0.5) % 120;
    }, 50);
  }

  // Calibration progress bar
  let progress = 0;
  setInterval(() => {
    const bar = document.getElementById("progress-bar");
    const text = document.getElementById("progress-text");
    
    if (bar && text) {
      progress = (progress + 1) % 100;
      bar.style.width = progress + "%";
      text.textContent = progress + "%  Building baseline";
    }
  }, 80);

  // Animate dots for status text
  const dots = ["", ".", "..", "...", "..", "."];
  let dotIdx = 0;
  
  setInterval(() => {
    const warmupDots = document.getElementById("dots-warmup");
    const calibDots = document.getElementById("dots-calib");
    const rollingDots = document.getElementById("dots-rolling");
    
    if (warmupDots) warmupDots.textContent = dots[dotIdx % dots.length];
    if (calibDots) calibDots.textContent = dots[dotIdx % dots.length];
    if (rollingDots) rollingDots.textContent = dots[dotIdx % dots.length];
    
    dotIdx += 1;
  }, 350);

  // Feature readout page rotation
  const features = [
    { label: "RMS", val: "245.8", label2: "PEAK Hz", val2: "1247" },
    { label: "KURT", val: "3.24", label2: "CREST", val2: "4.82" },
    { label: "CENTROID", val: "847", label2: "ZCR", val2: "0.168" },
  ];
  
  let featureIdx = 0;
  
  setInterval(() => {
    const feat = features[featureIdx % features.length];
    const pageNum = document.getElementById("page-num");
    const rmsVal = document.getElementById("rms-val");
    const hzVal = document.getElementById("hz-val");
    
    if (pageNum) pageNum.textContent = (featureIdx % features.length) + 1;
    if (rmsVal) rmsVal.textContent = feat.val;
    if (hzVal) hzVal.textContent = feat.val2;
    
    featureIdx += 1;
  }, 2000);
}

/**
 * Initialize all effects on DOM ready
 */
window.addEventListener("DOMContentLoaded", () => {
  // Small delay to ensure smooth initial render
  requestAnimationFrame(() => {
    initHeroStatusAnimation();
    initRevealAnimations();
    initCharts();
    initSmoothAnchors();
    initOledSimulations();
    initCodeHighlighting();
    initAOS();
  });
});

/**
 * Add enhanced scroll effect to topbar
 * Increases blur and shadow on scroll
 */
window.addEventListener("scroll", () => {
  const topbar = document.querySelector(".topbar");
  if (!topbar) return;
  
  const scrollY = window.scrollY;
  const maxBlur = 20;
  const blurIntensity = Math.min(scrollY / 500, 1);
  const blur = 16 + (blurIntensity * (maxBlur - 16));
  
  if (scrollY > 50) {
    topbar.style.boxShadow = "var(--shadow-mid)";
    topbar.style.backdropFilter = `blur(${blur}px)`;
    topbar.style.webkitBackdropFilter = `blur(${blur}px)`;
    topbar.style.backgroundColor = `rgba(248, 250, 246, ${0.7 + (blurIntensity * 0.2)})`;
  } else {
    topbar.style.boxShadow = "var(--shadow-soft)";
    topbar.style.backdropFilter = "blur(16px)";
    topbar.style.webkitBackdropFilter = "blur(16px)";
    topbar.style.backgroundColor = "rgba(248, 250, 246, 0.7)";
  }
});
