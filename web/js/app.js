const API = "";
let cachedUsers = [];

function formatBytes(n) {
  if (!n || n === 0) return "0 B";
  const u = ["B", "KB", "MB", "GB", "TB"];
  let i = 0;
  let v = n;
  while (v >= 1024 && i < u.length - 1) {
    v /= 1024;
    i++;
  }
  return `${v.toFixed(1)} ${u[i]}`;
}

function parseLimit(s) {
  if (!s) return 0;
  const m = String(s).trim().toLowerCase().match(/^([\d.]+)\s*(tb|gb|mb|kb|b)?$/);
  if (!m) return 0;
  const v = parseFloat(m[1]);
  const mul = { tb: 1e12, gb: 1e9, mb: 1e6, kb: 1e3, b: 1 };
  return Math.floor(v * (mul[m[2]] || mul.gb));
}

function formatUptime(sec) {
  if (!sec) return "—";
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (d > 0) return `${d}д ${h}ч`;
  if (h > 0) return `${h}ч ${m}м`;
  return `${m}м`;
}

function trafficPercent(used, limit) {
  if (!limit) return 0;
  return Math.min(100, (100 * used) / limit);
}

async function api(path, opts = {}) {
  const res = await fetch(API + path, {
    headers: { "Content-Type": "application/json", ...opts.headers },
    ...opts,
  });
  if (!res.ok) throw new Error(await res.text());
  const text = await res.text();
  return text ? JSON.parse(text) : {};
}

function setProgress(barEl, pct) {
  if (!barEl) return;
  const v = Math.max(0, Math.min(100, pct));
  barEl.style.width = `${v}%`;
  barEl.classList.toggle("warn", v >= 75 && v < 90);
  barEl.classList.toggle("danger", v >= 90);
}

// Theme
const themeToggle = document.getElementById("themeToggle");
const savedTheme = localStorage.getItem("grp-theme") || "dark";
document.documentElement.setAttribute("data-theme", savedTheme);

themeToggle?.addEventListener("click", () => {
  const next = document.documentElement.getAttribute("data-theme") === "dark" ? "light" : "dark";
  document.documentElement.setAttribute("data-theme", next);
  localStorage.setItem("grp-theme", next);
});

document.querySelectorAll(".nav-item").forEach((el) => {
  el.addEventListener("click", (e) => {
    e.preventDefault();
    const page = el.dataset.page;
    document.querySelectorAll(".nav-item").forEach((n) => n.classList.remove("active"));
    el.classList.add("active");
    document.querySelectorAll(".page").forEach((p) => p.classList.remove("active"));
    document.getElementById(`page-${page}`)?.classList.add("active");
    const titles = {
      dashboard: "Дашборд",
      users: "Пользователи",
      analytics: "Аналитика",
      alerts: "Алерты",
      server: "Сервер",
      provision: "Установка",
    };
    document.getElementById("pageTitle").textContent = titles[page] || page;
    if (page === "analytics") window.GRPAnalytics?.load();
    refresh();
  });
});

async function loadSystemStats() {
  try {
    const s = await api("/api/system/stats");
    const cpu = s.cpu_percent ?? 0;
    const ram = s.ram_percent ?? 0;

    document.getElementById("statCpu").textContent = `${cpu.toFixed(1)}%`;
    document.getElementById("statRam").textContent = `${ram.toFixed(1)}%`;

    document.getElementById("metricCpuVal").textContent = `${cpu.toFixed(1)}%`;
    document.getElementById("metricRamVal").textContent = `${ram.toFixed(1)}%`;
    document.getElementById("metricDiskVal").textContent = `${(s.disk_percent ?? 0).toFixed(1)}%`;
    document.getElementById("metricUptime").textContent = formatUptime(s.uptime_sec);
    document.getElementById("metricLoad").textContent = (s.load_avg_1 ?? 0).toFixed(2);
    document.getElementById("metricRamSub").textContent = `${s.ram_used_mb ?? 0} / ${s.ram_total_mb ?? 0} MB`;

    setProgress(document.getElementById("metricCpuBar"), cpu);
    setProgress(document.getElementById("metricRamBar"), ram);
    setProgress(document.getElementById("metricDiskBar"), s.disk_percent ?? 0);
  } catch (e) {
    console.warn("system stats", e);
  }
}

async function loadHealth() {
  const items = await api("/api/health");
  const alive = items.filter((i) => i.status === "alive").length;
  document.getElementById("healthPill").textContent = `${alive}/${items.length} сервисов`;
  document.getElementById("statServices")?.textContent =
    items.map((i) => `${i.service}: ${i.status}`).join(" · ") || "—";

  const grid = document.getElementById("healthGrid");
  if (grid) {
    grid.innerHTML = items
      .map(
        (i) => `
      <div class="health-card ${i.status}">
        <strong>${i.service}</strong>
        <p>${i.status}</p>
        <small>${i.last_check || "—"}</small>
      </div>`
      )
      .join("");
  }
}

function renderUsers(users) {
  const grid = document.getElementById("usersGrid");
  if (!grid) return;

  if (users.length === 0) {
    grid.innerHTML = `<p class="empty-state">Нет пользователей. Нажмите «+ Добавить».</p>`;
    return;
  }

  grid.innerHTML = users
    .map((u) => {
      const pct = trafficPercent(u.used, u.limit);
      const limitLabel = u.limit ? formatBytes(u.limit) : "∞";
      const devLabel =
        u.device_limit > 0 ? `${u.device_count || 0} / ${u.device_limit}` : "∞ устройств";
      return `
    <article class="user-card ${u.enabled ? "" : "disabled"}">
      <div class="user-card-head">
        <div>
          <h3>${escapeHtml(u.name)}</h3>
          <span class="user-uuid" title="${u.uuid}">${u.uuid.slice(0, 8)}…</span>
        </div>
        <label class="toggle" title="Вкл/выкл">
          <input type="checkbox" data-toggle="${escapeHtml(u.name)}" ${u.enabled ? "checked" : ""} />
          <span class="toggle-slider"></span>
        </label>
      </div>
      <div class="traffic-bar-wrap">
        <div class="traffic-bar"><div class="traffic-fill" style="width:${pct}%"></div></div>
        <span class="traffic-label">${formatBytes(u.used)} / ${limitLabel}</span>
      </div>
      <div class="user-meta">
        <span>📱 ${devLabel}</span>
        <span>${pct.toFixed(0)}% трафика</span>
      </div>
      <div class="user-actions">
        <button type="button" class="btn ghost btn-sm" data-reset="${escapeHtml(u.name)}">Сброс трафика</button>
        <button type="button" class="btn ghost btn-sm" data-copy="${escapeHtml(u.uuid)}">UUID</button>
        <button type="button" class="btn ghost btn-sm" data-link="${escapeHtml(u.name)}">Ссылка</button>
        <button type="button" class="btn ghost btn-sm danger" data-del="${escapeHtml(u.name)}">Удалить</button>
      </div>
    </article>`;
    })
    .join("");

  grid.querySelectorAll("[data-toggle]").forEach((inp) => {
    inp.addEventListener("change", async () => {
      await api(`/api/users/${encodeURIComponent(inp.dataset.toggle)}`, {
        method: "PATCH",
        body: JSON.stringify({ enabled: inp.checked }),
      });
      refresh();
    });
  });

  grid.querySelectorAll("[data-reset]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      if (!confirm(`Сбросить трафик для ${btn.dataset.reset}?`)) return;
      await api(`/api/users/${encodeURIComponent(btn.dataset.reset)}/reset-traffic`, {
        method: "POST",
      });
      refresh();
    });
  });

  grid.querySelectorAll("[data-copy]").forEach((btn) => {
    btn.addEventListener("click", () => {
      navigator.clipboard.writeText(btn.dataset.copy);
      btn.textContent = "Скопировано";
      setTimeout(() => (btn.textContent = "UUID"), 1200);
    });
  });

  grid.querySelectorAll("[data-link]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const r = await api(`/api/users/${encodeURIComponent(btn.dataset.link)}/link`);
      await navigator.clipboard.writeText(r.subscription);
      btn.textContent = "OK";
      setTimeout(() => (btn.textContent = "Ссылка"), 1200);
    });
  });

  grid.querySelectorAll("[data-del]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      if (!confirm(`Удалить ${btn.dataset.del}?`)) return;
      await api(`/api/users/${encodeURIComponent(btn.dataset.del)}`, { method: "DELETE" });
      refresh();
    });
  });
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

async function loadUsers() {
  cachedUsers = await api("/api/users");
  const active = cachedUsers.filter((u) => u.enabled).length;
  document.getElementById("statUsers").textContent = `${active} / ${cachedUsers.length}`;

  const q = (document.getElementById("userSearch")?.value || "").trim().toLowerCase();
  const filtered = q
    ? cachedUsers.filter((u) => u.name.toLowerCase().includes(q) || u.uuid.includes(q))
    : cachedUsers;
  renderUsers(filtered);
}

async function loadAlerts() {
  const alerts = await api("/api/alerts");
  document.getElementById("statAlerts").textContent = alerts.length;
  const list = document.getElementById("alertsList");
  if (!list) return;
  list.innerHTML =
    alerts.length === 0
      ? "<li>Нет активных алертов</li>"
      : alerts
          .map(
            (a) => `
    <li class="${a.severity}">
      <strong>${a.type}</strong> — ${a.message}
      <br><small>${a.created_at}</small>
    </li>`
          )
          .join("");
}

async function refresh() {
  try {
    await Promise.all([loadHealth(), loadUsers(), loadAlerts(), loadSystemStats()]);
    const dash = await api("/api/analytics/dashboard");
    window.GRPAnalytics?.renderDashboard(dash);
  } catch (e) {
    console.error(e);
  }
}

document.getElementById("btnRefresh")?.addEventListener("click", refresh);
document.getElementById("userSearch")?.addEventListener("input", () => {
  const q = (document.getElementById("userSearch")?.value || "").trim().toLowerCase();
  const filtered = q
    ? cachedUsers.filter((u) => u.name.toLowerCase().includes(q) || u.uuid.includes(q))
    : cachedUsers;
  renderUsers(filtered);
});

document.getElementById("btnExportUsers")?.addEventListener("click", () => {
  const blob = new Blob([JSON.stringify(cachedUsers, null, 2)], { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "ghostroute-users.json";
  a.click();
});

const userDialog = document.getElementById("userDialog");
const userForm = document.getElementById("userForm");

document.getElementById("btnAddUser")?.addEventListener("click", () => {
  document.getElementById("userDialogTitle").textContent = "Новый пользователь";
  userForm.reset();
  userForm.querySelector('[name="device_limit"]').value = "0";
  userDialog.showModal();
});

document.getElementById("userDialogCancel")?.addEventListener("click", () => userDialog.close());

userForm?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(userForm);
  await api("/api/users", {
    method: "POST",
    body: JSON.stringify({
      name: fd.get("name"),
      limit_bytes: parseLimit(fd.get("limit")),
      device_limit: parseInt(fd.get("device_limit") || "0", 10),
    }),
  });
  userDialog.close();
  refresh();
});

document.getElementById("provisionForm")?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  const log = document.getElementById("provisionLog");
  log.textContent = "Установка…";
  try {
    const r = await api("/api/provision/full", {
      method: "POST",
      body: JSON.stringify({ domain: fd.get("domain"), email: fd.get("email") }),
    });
    log.textContent = r.ok ? "Готово!" : "Ошибка установки";
  } catch (err) {
    log.textContent = String(err);
  }
});

refresh();
setInterval(refresh, 30000);
setInterval(loadSystemStats, 5000);
