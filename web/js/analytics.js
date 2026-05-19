window.GRPAnalytics = (() => {
  let hourlyChart;
  let countryChart;

  const countryNames = {
    US: "🇺🇸 США", DE: "🇩🇪 Германия", NL: "🇳🇱 Нидерланды", FI: "🇫🇮 Финляндия",
    SG: "🇸🇬 Сингапур", JP: "🇯🇵 Япония", RU: "🇷🇺 Россия", XX: "❓ Неизвестно",
  };

  function renderCountryMap(countries) {
    const el = document.getElementById("countryMap");
    if (!el) return;
    if (!countries?.length) {
      el.innerHTML = "<p class='muted'>Нет данных — подключите GeoLite2-Country.mmdb</p>";
      return;
    }
    const max = Math.max(...countries.map((c) => c.bytes), 1);
    el.innerHTML = countries
      .map((c) => {
        const pct = Math.round((c.bytes / max) * 100);
        const label = countryNames[c.code] || c.code;
        return `<span class="country-chip" style="opacity:${0.4 + pct / 166}">
          <strong>${label}</strong> ${formatBytes(c.bytes)} (${pct}%)
        </span>`;
      })
      .join("");
  }

  function formatBytes(n) {
    if (!n) return "0 B";
    const u = ["B", "KB", "MB", "GB"];
    let i = 0;
    while (n >= 1024 && i < 3) { n /= 1024; i++; }
    return `${n.toFixed(1)} ${u[i]}`;
  }

  function renderDashboard(data) {
    const hours = data.hourly || [];
    const labels = hours.map((h) => h.hour?.slice(11, 16) || h.hour);
    const totals = hours.map((h) => h.total || h.up + h.down);

    const ctx = document.getElementById("hourlyChart");
    if (ctx) {
      if (hourlyChart) hourlyChart.destroy();
      hourlyChart = new Chart(ctx, {
        type: "line",
        data: {
          labels,
          datasets: [{
            label: "Трафик",
            data: totals,
            borderColor: "#7c5cff",
            backgroundColor: "rgba(124,92,255,0.15)",
            fill: true,
            tension: 0.35,
          }],
        },
        options: {
          responsive: true,
          plugins: { legend: { display: false } },
          scales: {
            x: { grid: { color: "rgba(128,128,128,0.1)" } },
            y: { grid: { color: "rgba(128,128,128,0.1)" } },
          },
        },
      });
    }

    let traffic24 = 0;
    totals.forEach((t) => (traffic24 += t));
    const statTraffic = document.getElementById("statTraffic");
    if (statTraffic) statTraffic.textContent = formatBytes(traffic24);

    const topList = document.getElementById("topUsersList");
    if (topList) {
      topList.innerHTML = (data.top_users || [])
        .map((u, i) => `<li><span>${i + 1}. ${u.name}</span><span>${formatBytes(u.used)}</span></li>`)
        .join("") || "<li>Нет пользователей</li>";
    }

    renderCountryMap(data.countries);

    const cctx = document.getElementById("countryChart");
    if (cctx && data.countries?.length) {
      if (countryChart) countryChart.destroy();
      countryChart = new Chart(cctx, {
        type: "doughnut",
        data: {
          labels: data.countries.map((c) => c.code),
          datasets: [{
            data: data.countries.map((c) => c.bytes),
            backgroundColor: ["#7c5cff", "#00d4aa", "#ffb020", "#ff5c7a", "#5b8def"],
          }],
        },
        options: { responsive: true },
      });
    }
  }

  async function load() {
    try {
      const data = await fetch("/api/analytics/dashboard").then((r) => r.json());
      renderDashboard(data);
    } catch (e) {
      console.error(e);
    }
  }

  return { renderDashboard, load };
})();
