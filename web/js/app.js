const API = "";

function formatBytes(n) {
  if (!n || n === 0) return "∞";
  const u = ["B", "KB", "MB", "GB", "TB"];
  let i = 0;
  while (n >= 1024 && i < u.length - 1) {
    n /= 1024;
    i++;
  }
  return `${n.toFixed(1)} ${u[i]}`;
}

function parseLimit(s) {
  if (!s) return 0;
  const m = String(s).trim().toLowerCase().match(/^([\d.]+)\s*(tb|gb|mb|kb|b)?$/);
  if (!m) return 0;
  const v = parseFloat(m[1]);
  const mul = { tb: 1e12, gb: 1e9, mb: 1e6, kb: 1e3, b: 1 };
  return Math.floor(v * (mul[m[2]] || mul.gb));
}

async function api(path, opts = {}) {
  const res = await fetch(API + path, {
    headers: { "Content-Type": "application/json", ...opts.headers },
    ...opts,
  });
  if (!res.ok) throw new Error(await res.text());
  return res.json();
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

// Navigation
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

async function loadHealth() {
  const items = await api("/api/health");
  const alive = items.filter((i) => i.status === "alive").length;
  document.getElementById("healthPill").textContent = `${alive}/${items.length} сервисов`;
  document.getElementById("statServices").textContent =
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

async function loadUsers() {
  const users = await api("/api/users");
  document.getElementById("statUsers").textContent = users.length;

  const tbody = document.querySelector("#usersTable tbody");
  if (!tbody) return;
  tbody.innerHTML = users
    .map(
      (u) => `
    <tr>
      <td>${u.name}</td>
      <td>${formatBytes(u.used)}</td>
      <td>${u.limit ? formatBytes(u.limit) : "∞"}</td>
      <td>${u.enabled ? "✓" : "✗"}</td>
      <td><button class="btn ghost" data-del="${u.name}">Удалить</button></td>
    </tr>`
    )
    .join("");

  tbody.querySelectorAll("[data-del]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      await api(`/api/users/${encodeURIComponent(btn.dataset.del)}`, { method: "DELETE" });
      refresh();
    });
  });
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
    await Promise.all([loadHealth(), loadUsers(), loadAlerts()]);
    const dash = await api("/api/analytics/dashboard");
    window.GRPAnalytics?.renderDashboard(dash);
  } catch (e) {
    console.error(e);
  }
}

document.getElementById("btnRefresh")?.addEventListener("click", refresh);

const userDialog = document.getElementById("userDialog");
document.getElementById("btnAddUser")?.addEventListener("click", () => userDialog?.showModal());

document.getElementById("userForm")?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  const name = fd.get("name");
  const limit = parseLimit(fd.get("limit"));
  await api("/api/users", {
    method: "POST",
    body: JSON.stringify({ name, limit_bytes: limit }),
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
setInterval(refresh, 60000);
