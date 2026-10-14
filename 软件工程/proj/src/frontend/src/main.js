import { api } from "./api/client.js";
import { clearSession, setSession, state } from "./store/user.js";

const app = document.querySelector("#app");

/* ---- Helpers ---- */
function roleLabel(r) { return r >= 1 ? "管理员" : "普通用户"; }

function toast(text, type = "info") {
  const n = document.createElement("div");
  n.className = `toast ${type}`;
  n.textContent = text;
  document.body.appendChild(n);
  setTimeout(() => n.remove(), 2500);
}

function stars(rating) {
  return "★".repeat(rating) + "☆".repeat(5 - rating);
}

function optionsHtml(items = [], label = "全部") {
  return `<option value="">${label}</option>${items.map(i => `<option value="${i}">${i}</option>`).join("")}`;
}

function tagsHtml(tags = []) {
  if (!tags.length) return "";
  return `<div class="tag-list">${tags.map(t => `<span class="tag">${t}</span>`).join("")}</div>`;
}

function avatarLetter(name) {
  return (name || "?").charAt(0).toUpperCase();
}

function stripEmpty(obj) {
  return Object.fromEntries(Object.entries(obj).filter(([_, v]) => v !== "" && v != null));
}

/* ---- AI Chat State (in-memory) ---- */
const chatState = { open: false, messages: [], seed: 0 };

/* ---- Layout ---- */
function nav() {
  const u = state.user;
  const links = [
    { href: "#/", label: "首页" },
    { href: "#/rankings", label: "排行榜" },
  ];
  if (u && u.role >= 1) links.push({ href: "#/admin", label: "管理后台" });
  links.push({ href: "#/profile", label: "个人中心" });

  const route = location.hash.slice(1) || "/";
  const navHtml = links.map(l =>
    `<a href="${l.href}" class="${route === l.href.slice(1) || route.startsWith(l.href.slice(1) + '/') ? 'active' : ''}">${l.label}</a>`
  ).join("");

  const authPart = u
    ? `<a href="#/profile" class="nav-avatar" title="${u.username}">${avatarLetter(u.username)}</a>
       <button data-action="logout">退出</button>`
    : `<a href="#/login">登录</a>`;

  return `
    <header class="topbar">
      <div class="logo">西交食堂<small>XJTU Canteen Review</small></div>
      <nav>${navHtml}${authPart}</nav>
    </header>`;
}

function aiFab() {
  if (!state.user) return "";
  return `<button class="ai-fab" id="ai-fab" title="AI 美食助手">🤖</button>`;
}

function aiPanelHtml() {
  const msgs = chatState.messages.map(m => {
    if (m.type === "bot") {
      let content = `<div>${m.text}</div>`;
      if (m.cards && m.cards.length) {
        content += m.cards.map(c => `
          <div class="rec-mini-card">
            <h4>${c.stall_name} <span class="text-sm text-muted">${c.canteen_name}</span></h4>
            <p>${stars(Math.round(c.avg_rating))} ${Number(c.avg_rating).toFixed(1)} · ${c.review_count}条评价</p>
            ${c.reason ? `<p>${c.reason}</p>` : ""}
            <a href="#/stall/${c.stall_id}">查看详情 →</a>
          </div>`).join("");
      }
      return `<div class="ai-msg bot">${content}</div>`;
    }
    return `<div class="ai-msg user">${m.text}</div>`;
  }).join("");

  return `
    <div class="ai-panel" id="ai-panel">
      <div class="ai-panel-header">
        <h3>🤖 AI 美食助手</h3>
        <button class="modal-close" id="ai-close">✕</button>
      </div>
      <div class="ai-messages" id="ai-messages">${msgs}</div>
      <div class="ai-input-bar">
        <input id="ai-input" type="text" placeholder="告诉我你想吃什么..." />
        <button id="ai-send">➤</button>
      </div>
    </div>`;
}

function renderLayout(content) {
  app.innerHTML = `${nav()}<main class="page"><div class="wrap">${content}</div></main>${aiFab()}`;

  const logoutBtn = app.querySelector('[data-action="logout"]');
  if (logoutBtn) {
    logoutBtn.onclick = async () => {
      await api.logout();
      clearSession();
      chatState.messages = [];
      location.hash = "#/login";
    };
  }

  const fab = document.getElementById("ai-fab");
  if (fab) {
    fab.onclick = () => openAiPanel();
  }
}

/* ---- AI Panel Logic ---- */
function openAiPanel() {
  chatState.open = true;
  if (chatState.messages.length === 0) {
    const pref = state.user?.preference_text;
    chatState.messages.push({
      type: "bot",
      text: pref
        ? `你好！我是你的 AI 美食助手 🍜 我注意到你喜欢「${pref}」，告诉我今天想吃什么吧！`
        : "你好！我是你的 AI 美食助手 🍜 告诉我你想吃什么，我来帮你推荐！",
      cards: []
    });
  }
  // inject panel
  let panel = document.getElementById("ai-panel");
  if (panel) panel.remove();
  document.body.insertAdjacentHTML("beforeend", aiPanelHtml());

  const msgBox = document.getElementById("ai-messages");
  msgBox.scrollTop = msgBox.scrollHeight;

  document.getElementById("ai-close").onclick = closeAiPanel;
  document.getElementById("ai-send").onclick = sendAiMessage;
  document.getElementById("ai-input").onkeydown = (e) => {
    if (e.key === "Enter") sendAiMessage();
  };

  // hide fab
  const fab = document.getElementById("ai-fab");
  if (fab) fab.classList.add("hidden");
}

function closeAiPanel() {
  const panel = document.getElementById("ai-panel");
  if (panel) {
    panel.classList.add("closing");
    setTimeout(() => panel.remove(), 250);
  }
  chatState.open = false;
  const fab = document.getElementById("ai-fab");
  if (fab) fab.classList.remove("hidden");
}

async function sendAiMessage() {
  const input = document.getElementById("ai-input");
  const text = (input?.value || "").trim();
  if (!text) return;
  input.value = "";

  chatState.messages.push({ type: "user", text });
  refreshAiMessages();

  chatState.seed = Math.floor(Math.random() * 100);
  try {
    const result = await api.recommendFeed({
      preference_text: text,
      limit: 3,
      seed: chatState.seed,
      exclude_blacklist: true,
    });
    if (result.code === 0 && result.data.list.length) {
      const summary = result.data.ai_summary || "根据你的需求，我找到了这些推荐：";
      chatState.messages.push({ type: "bot", text: summary, cards: result.data.list });
    } else {
      chatState.messages.push({ type: "bot", text: "没找到特别合适的，换个描述试试？比如说说想吃辣的还是清淡的。", cards: [] });
    }
  } catch {
    chatState.messages.push({ type: "bot", text: "网络出了点问题，稍后再试吧。", cards: [] });
  }
  refreshAiMessages();
}

function refreshAiMessages() {
  const msgBox = document.getElementById("ai-messages");
  if (!msgBox) return;
  const msgs = chatState.messages.map(m => {
    if (m.type === "bot") {
      let content = `<div>${m.text}</div>`;
      if (m.cards && m.cards.length) {
        content += m.cards.map(c => `
          <div class="rec-mini-card">
            <h4>${c.stall_name} <span class="text-sm text-muted">${c.canteen_name}</span></h4>
            <p>${stars(Math.round(c.avg_rating))} ${Number(c.avg_rating).toFixed(1)} · ${c.review_count}条评价</p>
            ${c.reason ? `<p>${c.reason}</p>` : ""}
            <a href="#/stall/${c.stall_id}">查看详情 →</a>
          </div>`).join("");
      }
      return `<div class="ai-msg bot">${content}</div>`;
    }
    return `<div class="ai-msg user">${m.text}</div>`;
  }).join("");
  msgBox.innerHTML = msgs;
  msgBox.scrollTop = msgBox.scrollHeight;
}

/* ---- Shared: Tab System ---- */
function initTabs(container, tabsConfig, defaultTab) {
  const tabBar = container.querySelector(".tabs");
  const tabContent = container.querySelector(".tab-content");
  if (!tabBar || !tabContent) return;

  const buttons = tabBar.querySelectorAll("button[data-tab]");
  const indicator = tabBar.querySelector(".tab-indicator");

  function activate(key) {
    buttons.forEach(b => b.classList.toggle("active", b.dataset.tab === key));
    if (indicator) {
      const activeBtn = tabBar.querySelector(`button[data-tab="${key}"]`);
      if (activeBtn) {
        indicator.style.left = activeBtn.offsetLeft + "px";
        indicator.style.width = activeBtn.offsetWidth + "px";
      }
    }
    const cfg = tabsConfig[key];
    if (cfg) {
      tabContent.innerHTML = `<div class="tab-content">${typeof cfg.html === "function" ? cfg.html() : cfg.html}</div>`;
      if (cfg.init) cfg.init(tabContent);
    }
  }

  buttons.forEach(b => {
    b.onclick = () => activate(b.dataset.tab);
  });

  activate(defaultTab || buttons[0]?.dataset.tab);
}

/* ---- Ensure Auth ---- */
async function ensureMe() {
  if (!state.token) return;
  try {
    const r = await api.me();
    if (r.code === 0) setSession(state.token, r.data);
    else clearSession();
  } catch { clearSession(); }
}

function requireLogin() {
  if (state.user) return true;
  location.hash = "#/login";
  return false;
}

function readFileAsDataUrl(file) {
  return new Promise((resolve, reject) => {
    const r = new FileReader();
    r.onload = () => resolve(r.result);
    r.onerror = reject;
    r.readAsDataURL(file);
  });
}

/* ---- Card Components ---- */
function stallCard(item) {
  return `
    <article class="card">
      <div class="card-head">
        <div>
          <h3>${item.name}</h3>
          <p>${item.canteen_name} · ${item.category || "未分类"}</p>
        </div>
        <div class="score-badge">${Number(item.avg_rating || 0).toFixed(1)}</div>
      </div>
      ${tagsHtml(item.tags || [])}
      <p class="card-desc">${item.description || "还没有简介"}</p>
      <div class="card-foot">
        <span>${item.review_count || 0} 条评价</span>
        <a href="#/stall/${item.id}">查看详情 →</a>
      </div>
    </article>`;
}

function reviewRow(item, opts = {}) {
  const { allowEdit = false, allowDelete = false, showStall = false } = opts;
  return `
    <article class="review-item">
      <div class="review-header">
        <div class="review-user">
          <div class="review-avatar">${avatarLetter(item.username || item.stall_name)}</div>
          <div>
            <strong>${showStall ? (item.stall_name || "") : (item.username || "匿名")}</strong>
            ${showStall && item.canteen_name ? `<span class="text-sm text-muted"> · ${item.canteen_name}</span>` : ""}
          </div>
        </div>
        <div class="review-stars">${stars(item.rating)}</div>
      </div>
      <p class="review-content">${item.content || "这位同学没有留下文字评价"}</p>
      <div class="review-meta">${item.updated_at || item.created_at}</div>
      ${allowEdit || allowDelete ? `
        <div class="review-actions">
          ${allowEdit ? `<button class="btn-sm btn-ghost" data-edit-review="${item.id}">修改</button>` : ""}
          ${allowDelete ? `<button class="btn-sm btn-ghost" data-delete-review="${item.id}">删除</button>` : ""}
        </div>` : ""}
    </article>`;
}

/* ---- Login ---- */
async function renderLogin() {
  if (state.user) { location.hash = "#/"; return; }
  app.innerHTML = `${nav()}
    <div class="auth-page">
      <div class="auth-card">
        <h2>欢迎回来</h2>
        <p class="auth-sub">登录后享受评价、收藏与 AI 推荐功能</p>
        <form id="login-form" class="form-grid">
          <input name="student_id" placeholder="账号 / 学号" required />
          <input name="password" type="password" placeholder="密码" required />
          <button type="submit">登录</button>
        </form>
        <p class="auth-link">还没有账号？<a href="#/register">立即注册</a></p>
      </div>
    </div>`;

  app.querySelector("#login-form").onsubmit = async (e) => {
    e.preventDefault();
    const d = Object.fromEntries(new FormData(e.currentTarget).entries());
    const r = await api.login(d);
    if (r.code === 0) {
      setSession(r.data.token, r.data.user);
      toast("登录成功", "success");
      location.hash = "#/";
    } else {
      toast(r.message || "登录失败", "error");
    }
  };
}

/* ---- Register ---- */
async function renderRegister() {
  if (state.user) { location.hash = "#/"; return; }
  app.innerHTML = `${nav()}
    <div class="auth-page">
      <div class="auth-card">
        <h2>创建账号</h2>
        <p class="auth-sub">注册后即可评价窗口、收藏和获取推荐</p>
        <form id="reg-form" class="form-grid">
          <input name="student_id" placeholder="学号" required />
          <input name="username" placeholder="昵称" required />
          <input name="password" type="password" placeholder="密码" required />
          <button type="submit">注册</button>
        </form>
        <p class="auth-link">已有账号？<a href="#/login">去登录</a></p>
      </div>
    </div>`;

  app.querySelector("#reg-form").onsubmit = async (e) => {
    e.preventDefault();
    const d = Object.fromEntries(new FormData(e.currentTarget).entries());
    const r = await api.register(d);
    if (r.code === 0) {
      toast("注册成功，请登录", "success");
      location.hash = "#/login";
    } else {
      toast(r.message || "注册失败", "error");
    }
  };
}

/* ---- Home Page ---- */
async function renderHome() {
  const [canteens, categories, tags] = await Promise.all([api.canteens(), api.categories(), api.tags()]);
  renderLayout(`
    <div class="flex-between" style="flex-wrap:wrap;gap:12px;">
      <h2 class="section-title" style="margin:0;">窗口列表</h2>
      <button class="btn-golden" id="today-btn">🎲 今天吃什么？</button>
    </div>
    <div class="filter-bar" id="filter-bar">
      <select name="canteen_id">
        <option value="">全部食堂</option>
        ${(canteens.data.list || []).map(i => `<option value="${i.id}">${i.name}</option>`).join("")}
      </select>
      <select name="category">${optionsHtml(categories.data.list || [], "全部分类")}</select>
      <select name="tag_name">
        <option value="">全部标签</option>
        ${(tags.data.list || []).map(i => `<option value="${i.name}">${i.name}</option>`).join("")}
      </select>
      <input type="text" name="keyword" placeholder="搜索窗口名称..." />
      <select name="sort_by">
        <option value="">默认排序</option>
        <option value="score">评分优先</option>
        <option value="hot">热度优先</option>
      </select>
      <button id="search-btn">查询</button>
    </div>
    <section id="stall-list" class="grid"></section>
  `);

  let todaySeed = 0;

  async function loadStalls() {
    const bar = app.querySelector("#filter-bar");
    const params = {};
    bar.querySelectorAll("select, input").forEach(el => {
      if (el.name && el.value) params[el.name] = el.value;
    });
    const result = await api.stalls({ page: 1, page_size: 20, ...stripEmpty(params) });
    app.querySelector("#stall-list").innerHTML =
      result.code === 0 && result.data.list.length
        ? result.data.list.map(stallCard).join("")
        : `<div class="empty">没找到符合条件的窗口，换个筛选试试</div>`;
  }

  app.querySelector("#search-btn").onclick = loadStalls;
  // Also search on Enter in keyword input
  app.querySelector('#filter-bar input[name="keyword"]').onkeydown = (e) => {
    if (e.key === "Enter") { e.preventDefault(); loadStalls(); }
  };
  // Auto-search on select change
  app.querySelectorAll("#filter-bar select").forEach(sel => {
    sel.onchange = loadStalls;
  });

  // "今天吃什么" modal
  app.querySelector("#today-btn").onclick = () => openTodayModal();

  await loadStalls();
}

/* ---- "今天吃什么" Modal ---- */
let todaySeed = 0;
async function openTodayModal() {
  todaySeed = Math.floor(Math.random() * 50);
  showTodayResult();
}

async function showTodayResult() {
  // Show loading modal first
  let overlay = document.getElementById("today-overlay");
  if (!overlay) {
    document.body.insertAdjacentHTML("beforeend", `
      <div class="modal-overlay" id="today-overlay">
        <div class="modal-card" id="today-card">
          <button class="modal-close" id="today-close">✕</button>
          <div class="loading-spinner"></div>
          <p class="text-muted mt-sm">正在为你挑选...</p>
        </div>
      </div>`);
    overlay = document.getElementById("today-overlay");
    document.getElementById("today-close").onclick = closeTodayModal;
    overlay.onclick = (e) => { if (e.target === overlay) closeTodayModal(); };
  }

  try {
    const r = await api.recommendToday({ limit: 1, exclude_blacklist: true, seed: todaySeed });
    const card = document.getElementById("today-card");
    if (r.code === 0 && r.data.list && r.data.list.length) {
      const s = r.data.list[0];
      card.innerHTML = `
        <button class="modal-close" id="today-close">✕</button>
        <div style="font-size:2.5rem;margin-bottom:8px;">🍜</div>
        <h2 class="modal-stall-name">${s.stall_name}</h2>
        <p class="modal-stall-sub">${s.canteen_name} · ${s.category || "未分类"}</p>
        <div class="modal-score">${Number(s.avg_rating).toFixed(1)}</div>
        <p class="modal-score-label">${s.review_count} 条评价</p>
        ${tagsHtml(s.tags || [])}
        <p class="modal-reason">${s.reason || "根据评分和热度推荐给你"}</p>
        <div class="modal-actions">
          <button class="btn-golden" id="today-refresh">🎲 换一个</button>
          <a href="#/stall/${s.stall_id}" id="today-goto" class="btn-secondary" style="padding:10px 20px;border-radius:10px;color:#fff !important;display:inline-flex;align-items:center;text-decoration:none;font-weight:500;">去看看 →</a>
        </div>`;
      document.getElementById("today-close").onclick = closeTodayModal;
      document.getElementById("today-refresh").onclick = () => { todaySeed++; showTodayResult(); };
      document.getElementById("today-goto").onclick = () => closeTodayModal();
    } else {
      card.innerHTML = `
        <button class="modal-close" id="today-close">✕</button>
        <div style="font-size:2.5rem;">😅</div>
        <p class="mt-md text-muted">今天的窗口都被你拉黑了？试着移除几个吧</p>`;
      document.getElementById("today-close").onclick = closeTodayModal;
    }
  } catch {
    toast("推荐获取失败", "error");
    closeTodayModal();
  }
}

function closeTodayModal() {
  const o = document.getElementById("today-overlay");
  if (o) o.remove();
}

/* ---- Stall Detail ---- */
async function renderStall(id) {
  const [detail, reviews] = await Promise.all([
    api.stallDetail(id),
    api.stallReviews(id, { page: 1, page_size: 20 }),
  ]);

  if (detail.code !== 0) {
    renderLayout(`<div class="empty">窗口不存在</div>`);
    return;
  }

  // Auto-record history
  if (state.user) {
    api.addHistory({ stall_id: Number(id) }).catch(() => {});
  }

  const item = detail.data;
  renderLayout(`
    <div class="detail-header">
      <div class="detail-info">
        <h2>${item.name}</h2>
        <p class="subtitle">${item.canteen_name} · ${item.category || "未分类"}</p>
        ${tagsHtml(item.tags || [])}
        <p class="text-muted mt-sm">${item.description || "暂无简介"}</p>
        ${state.user ? `
          <div class="detail-actions mt-md">
            <button class="btn-icon" id="fav-btn" title="收藏">♥</button>
            <button class="btn-icon" id="black-btn" title="拉黑">✕</button>
          </div>` : ""}
      </div>
      <div class="detail-score">
        <strong>${Number(item.avg_rating || 0).toFixed(1)}</strong>
        <span>${item.review_count || 0} 条评价</span>
      </div>
    </div>

    <div class="panel">
      <h3 class="section-title">评论列表</h3>
      <div id="review-list">
        ${reviews.data.list.length
          ? reviews.data.list.map(x => reviewRow(x)).join("")
          : `<div class="empty">还没有人评价过，来写第一条吧</div>`}
      </div>
    </div>

    ${state.user ? `
    <div class="panel">
      <h3 class="section-title">写评论</h3>
      <form id="review-form" class="form-grid">
        <div>
          <label class="form-label">评分</label>
          <div class="star-input" id="star-input">
            ${[1,2,3,4,5].map(i => `<span data-val="${i}">☆</span>`).join("")}
          </div>
          <input type="hidden" name="rating" value="5" />
        </div>
        <textarea name="content" placeholder="分享你的用餐体验..."></textarea>
        <button type="submit">提交评论</button>
      </form>
    </div>` : `<div class="panel"><div class="empty">登录后可以写评论</div></div>`}
  `);

  // Star rating interaction
  const starInput = app.querySelector("#star-input");
  if (starInput) {
    const ratingInput = app.querySelector('input[name="rating"]');
    let currentRating = 5;
    function updateStars(val) {
      starInput.querySelectorAll("span").forEach((s, i) => {
        s.textContent = i < val ? "★" : "☆";
        s.classList.toggle("filled", i < val);
      });
    }
    updateStars(5);
    starInput.querySelectorAll("span").forEach(s => {
      s.onmouseenter = () => updateStars(Number(s.dataset.val));
      s.onclick = () => { currentRating = Number(s.dataset.val); ratingInput.value = currentRating; updateStars(currentRating); };
    });
    starInput.onmouseleave = () => updateStars(currentRating);
  }

  // Favorite & Blacklist
  const favBtn = app.querySelector("#fav-btn");
  if (favBtn) {
    favBtn.onclick = async () => {
      const r = await api.addFavorite({ stall_id: Number(id) });
      toast(r.code === 0 ? "已收藏" : r.message, r.code === 0 ? "success" : "error");
      if (r.code === 0) favBtn.classList.add("active");
    };
  }
  const blackBtn = app.querySelector("#black-btn");
  if (blackBtn) {
    blackBtn.onclick = async () => {
      const r = await api.addBlacklist({ stall_id: Number(id) });
      toast(r.code === 0 ? "已拉黑" : r.message, r.code === 0 ? "success" : "error");
    };
  }

  // Review form
  const reviewForm = app.querySelector("#review-form");
  if (reviewForm) {
    reviewForm.onsubmit = async (e) => {
      e.preventDefault();
      const d = Object.fromEntries(new FormData(e.currentTarget).entries());
      const r = await api.submitReview({ stall_id: Number(id), rating: Number(d.rating), content: d.content });
      if (r.code === 0) { toast("评论已提交", "success"); await renderStall(id); }
      else toast(r.message, "error");
    };
  }
}

/* ---- Rankings ---- */
async function renderRankings() {
  renderLayout(`
    <h2 class="section-title">排行榜</h2>
    <div class="tabs" id="rank-tabs">
      <button data-tab="score" class="active">评分榜</button>
      <button data-tab="hot">热度榜</button>
      <button data-tab="latest">最新评价</button>
      <div class="tab-indicator"></div>
    </div>
    <div id="rank-content"></div>
  `);

  async function loadRank(tab) {
    const content = app.querySelector("#rank-content");
    content.innerHTML = `<div class="loading-spinner"></div>`;
    const map = { score: api.scoreRank, hot: api.hotRank, latest: api.latestRank };
    const r = await map[tab]({ limit: 10 });
    const list = r.data.list || [];
    if (!list.length) { content.innerHTML = `<div class="empty">还没有数据</div>`; return; }

    const top3 = list.slice(0, 3);
    const rest = list.slice(3);

    content.innerHTML = `
      <div class="rank-podium">
        ${top3.map((item, i) => `
          <article class="card" style="text-align:center;">
            <div class="rank-no">${i + 1}</div>
            <h3>${item.stall_name}</h3>
            <p class="text-sm text-muted">${item.canteen_name}</p>
            <div class="score-badge" style="margin:12px auto;">${Number(item.avg_rating || 0).toFixed(1)}</div>
            <p class="text-sm">${item.review_count || 0} 条评价</p>
            <a href="#/stall/${item.stall_id}" style="margin-top:8px;display:inline-block;">查看详情 →</a>
          </article>`).join("")}
      </div>
      <div style="display:flex;flex-direction:column;gap:8px;">
        ${rest.map((item, i) => `
          <div class="rank-list-item" style="animation-delay:${(i + 3) * 0.04}s">
            <div class="rank-no">${i + 4}</div>
            <div class="rank-info">
              <h4>${item.stall_name}</h4>
              <p>${item.canteen_name} · ${item.review_count || 0} 条评价</p>
            </div>
            <div class="score-badge">${Number(item.avg_rating || 0).toFixed(1)}</div>
            <a href="#/stall/${item.stall_id}">详情 →</a>
          </div>`).join("")}
      </div>`;
  }

  // Tab switching
  const tabs = app.querySelector("#rank-tabs");
  const indicator = tabs.querySelector(".tab-indicator");
  const buttons = tabs.querySelectorAll("button[data-tab]");

  function activateTab(btn) {
    buttons.forEach(b => b.classList.remove("active"));
    btn.classList.add("active");
    indicator.style.left = btn.offsetLeft + "px";
    indicator.style.width = btn.offsetWidth + "px";
    loadRank(btn.dataset.tab);
  }

  buttons.forEach(b => { b.onclick = () => activateTab(b); });
  // Init indicator position
  requestAnimationFrame(() => {
    const first = tabs.querySelector("button.active");
    if (first) { indicator.style.left = first.offsetLeft + "px"; indicator.style.width = first.offsetWidth + "px"; }
  });

  await loadRank("score");
}

/* ---- Profile Page (Tabbed) ---- */
async function renderProfile() {
  if (!requireLogin()) return;

  const [me, reviews, favorites, blacklist, history] = await Promise.all([
    api.me(),
    api.myReviews({ page: 1, page_size: 50 }),
    api.favorites({ page: 1, page_size: 50 }),
    api.blacklist({ page: 1, page_size: 50 }),
    api.history({ page: 1, page_size: 50 }),
  ]);

  const u = me.data;

  renderLayout(`
    <div class="profile-header">
      <img class="avatar-large" src="${u.avatar_url || `https://ui-avatars.com/api/?name=${encodeURIComponent(u.username)}&background=1A9E96&color=fff&size=80`}" alt="avatar" />
      <div class="profile-meta">
        <h2>${u.username}</h2>
        <p>${u.student_id} · ${roleLabel(u.role)}</p>
        <p>${u.signature || "这个人很懒，什么都没写"}</p>
      </div>
    </div>

    <div class="tabs" id="profile-tabs">
      <button data-tab="info" class="active">个人资料</button>
      <button data-tab="reviews">我的评论</button>
      <button data-tab="favs">收藏</button>
      <button data-tab="black">黑名单</button>
      <button data-tab="history">浏览历史</button>
      <div class="tab-indicator"></div>
    </div>
    <div id="profile-content" class="panel"></div>
  `);

  const tabsConfig = {
    info: {
      html: () => `
        <div class="profile-edit-layout">
          <!-- 左侧：头像卡片 -->
          <div class="profile-avatar-card">
            <div class="profile-avatar-wrap">
              <img id="avatar-preview-img"
                src="${u.avatar_url || `https://ui-avatars.com/api/?name=${encodeURIComponent(u.username)}&background=1A9E96&color=fff&size=160`}"
                alt="avatar" class="profile-avatar-img" />
              <label class="avatar-upload-btn" title="更换头像">
                📷
                <input id="avatar-file" type="file" accept="image/*" style="display:none" />
              </label>
            </div>
            <div class="profile-avatar-name">${u.username}</div>
            <div class="profile-avatar-role">${roleLabel(u.role)}</div>
            <div class="profile-avatar-id">${u.student_id}</div>
            <div class="profile-avatar-sig">${u.signature || "还没有签名"}</div>
          </div>

          <!-- 右侧：编辑表单 -->
          <div class="profile-edit-forms">
            <form id="profile-form">
              <div class="profile-section-title">基本信息</div>
              <div class="profile-form-row">
                <div class="profile-field">
                  <label class="form-label">学号</label>
                  <input name="student_id" value="${u.student_id || ""}" placeholder="学号" />
                </div>
                <div class="profile-field">
                  <label class="form-label">昵称</label>
                  <input name="username" value="${u.username || ""}" placeholder="昵称" />
                </div>
              </div>
              <div class="profile-field">
                <label class="form-label">个性签名</label>
                <input name="signature" value="${u.signature || ""}" placeholder="写点什么，展示在个人页上" />
              </div>
              <div class="profile-section-title" style="margin-top:20px;">AI 推荐偏好</div>
              <div class="profile-field">
                <label class="form-label">口味偏好 <span class="form-label-hint">AI 助手会参考这里来推荐</span></label>
                <textarea name="preference_text" placeholder="例如：喜欢辣的、不吃香菜、预算15以内">${u.preference_text || ""}</textarea>
              </div>
              <div class="profile-field" style="display:none">
                <input name="avatar_url" id="avatar-url-input" value="${u.avatar_url || ""}" />
              </div>
              <button type="submit" style="margin-top:8px;">保存资料</button>
            </form>

            <div class="profile-divider"></div>

            <form id="pw-form">
              <div class="profile-section-title">修改密码</div>
              <div class="profile-form-row">
                <div class="profile-field">
                  <label class="form-label">原密码</label>
                  <input name="old_password" type="password" placeholder="输入原密码" required />
                </div>
                <div class="profile-field">
                  <label class="form-label">新密码</label>
                  <input name="new_password" type="password" placeholder="输入新密码" required />
                </div>
              </div>
              <button type="submit" class="btn-ghost" style="margin-top:8px;">确认修改</button>
            </form>
          </div>
        </div>`,
      init: (container) => {
        // 头像文件选择预览
        const avatarFile = container.querySelector("#avatar-file");
        avatarFile.onchange = async () => {
          const file = avatarFile.files[0];
          if (!file) return;
          const dataUrl = await readFileAsDataUrl(file);
          container.querySelector("#avatar-preview-img").src = dataUrl;
          container.querySelector("#avatar-url-input").value = dataUrl;
        };

        container.querySelector("#profile-form").onsubmit = async (e) => {
          e.preventDefault();
          const payload = Object.fromEntries(new FormData(e.currentTarget).entries());
          // 如果有本地上传的头像，用 data URL 覆盖
          const avatarUrlInput = container.querySelector("#avatar-url-input");
          if (avatarUrlInput.value) payload.avatar_url = avatarUrlInput.value;
          const r = await api.updateProfile(payload);
          if (r.code === 0) { setSession(state.token, r.data); toast("资料已保存", "success"); await renderProfile(); }
          else toast(r.message, "error");
        };
        container.querySelector("#pw-form").onsubmit = async (e) => {
          e.preventDefault();
          const d = Object.fromEntries(new FormData(e.currentTarget).entries());
          const r = await api.changePassword(d);
          toast(r.code === 0 ? "密码已修改" : r.message, r.code === 0 ? "success" : "error");
        };
      }
    },
    reviews: {
      html: () => reviews.data.list.length
        ? reviews.data.list.map(x => reviewRow(x, { allowEdit: true, allowDelete: true, showStall: true })).join("")
        : `<div class="empty">还没写过评价，去逛逛窗口吧</div>`,
      init: (container) => {
        // Inline edit
        container.querySelectorAll("[data-edit-review]").forEach(btn => {
          btn.onclick = () => {
            const article = btn.closest(".review-item");
            const id = btn.dataset.editReview;
            const existing = article.querySelector(".review-content")?.textContent || "";
            const existingStars = article.querySelector(".review-stars")?.textContent?.match(/★/g)?.length || 5;
            article.querySelector(".review-actions").innerHTML = `
              <div class="form-grid" style="max-width:100%;margin-top:8px;">
                <select id="edit-rating-${id}">
                  ${[5,4,3,2,1].map(v => `<option value="${v}" ${v === existingStars ? "selected" : ""}>${v} 分</option>`).join("")}
                </select>
                <textarea id="edit-content-${id}">${existing}</textarea>
                <div style="display:flex;gap:8px;">
                  <button class="btn-sm" id="save-review-${id}">保存</button>
                  <button class="btn-sm btn-ghost" id="cancel-review-${id}">取消</button>
                </div>
              </div>`;
            document.getElementById(`save-review-${id}`).onclick = async () => {
              const r = await api.updateMyReview(id, {
                rating: Number(document.getElementById(`edit-rating-${id}`).value),
                content: document.getElementById(`edit-content-${id}`).value,
              });
              toast(r.code === 0 ? "已更新" : r.message, r.code === 0 ? "success" : "error");
              if (r.code === 0) await renderProfile();
            };
            document.getElementById(`cancel-review-${id}`).onclick = () => renderProfile();
          };
        });
        container.querySelectorAll("[data-delete-review]").forEach(btn => {
          btn.onclick = async () => {
            const r = await api.deleteMyReview(btn.dataset.deleteReview);
            toast(r.code === 0 ? "已删除" : r.message, r.code === 0 ? "success" : "error");
            if (r.code === 0) await renderProfile();
          };
        });
      }
    },

    favs: {
      html: () => favorites.data.list.length
        ? `<div class="grid">${favorites.data.list.map(x => `
            <article class="card">
              <div class="card-head">
                <div><h3>${x.stall_name}</h3><p>${x.canteen_name}</p></div>
              </div>
              <div class="card-foot">
                <a href="#/stall/${x.stall_id}">查看详情 →</a>
                <button class="btn-sm btn-ghost" data-unfav="${x.stall_id}">取消收藏</button>
              </div>
            </article>`).join("")}</div>`
        : `<div class="empty">还没有收藏，去看看有没有喜欢的窗口</div>`,
      init: (container) => {
        container.querySelectorAll("[data-unfav]").forEach(btn => {
          btn.onclick = async () => {
            const r = await api.deleteFavorite(btn.dataset.unfav);
            toast(r.code === 0 ? "已取消收藏" : r.message, r.code === 0 ? "success" : "error");
            if (r.code === 0) await renderProfile();
          };
        });
      }
    },
    black: {
      html: () => blacklist.data.list.length
        ? blacklist.data.list.map(x => `
          <div class="rank-list-item">
            <div class="rank-info"><h4>${x.stall_name}</h4><p>${x.canteen_name}</p></div>
            <button class="btn-sm btn-ghost" data-unblack="${x.stall_id}">移除</button>
          </div>`).join("")
        : `<div class="empty">黑名单是空的，看来你对食堂还挺满意的</div>`,
      init: (container) => {
        container.querySelectorAll("[data-unblack]").forEach(btn => {
          btn.onclick = async () => {
            const r = await api.deleteBlacklist(btn.dataset.unblack);
            toast(r.code === 0 ? "已移除黑名单" : r.message, r.code === 0 ? "success" : "error");
            if (r.code === 0) await renderProfile();
          };
        });
      }
    },
    history: {
      html: () => history.data.list.length
        ? history.data.list.map(x => `
          <div class="rank-list-item">
            <div class="rank-info"><h4>${x.stall_name}</h4><p>${x.canteen_name}</p></div>
            <span class="text-sm text-muted">${x.visited_at}</span>
          </div>`).join("")
        : `<div class="empty">还没有浏览记录，去看看有什么好吃的吧</div>`,
    },
  };

  // Init tabs
  const tabBar = app.querySelector("#profile-tabs");
  const contentEl = app.querySelector("#profile-content");
  const tabBtns = tabBar.querySelectorAll("button[data-tab]");
  const tabInd = tabBar.querySelector(".tab-indicator");

  function switchTab(key) {
    tabBtns.forEach(b => b.classList.toggle("active", b.dataset.tab === key));
    const activeBtn = tabBar.querySelector(`button[data-tab="${key}"]`);
    if (activeBtn && tabInd) {
      tabInd.style.left = activeBtn.offsetLeft + "px";
      tabInd.style.width = activeBtn.offsetWidth + "px";
    }
    const cfg = tabsConfig[key];
    if (cfg) {
      contentEl.innerHTML = typeof cfg.html === "function" ? cfg.html() : cfg.html;
      if (cfg.init) cfg.init(contentEl);
    }
  }

  tabBtns.forEach(b => { b.onclick = () => switchTab(b.dataset.tab); });
  requestAnimationFrame(() => switchTab("info"));
}

/* ---- Admin Page (Tabbed) ---- */
async function renderAdmin() {
  if (!requireLogin()) return;
  if (state.user.role < 1) {
    renderLayout(`<div class="empty">你没有管理员权限，如需开通请联系管理员</div>`);
    return;
  }

  const [canteens, categories, stalls, tags, usersResult] = await Promise.all([
    api.canteens(),
    api.categories(),
    api.stalls({ page: 1, page_size: 100 }),
    api.adminTags(),
    api.adminUsers().catch(() => ({ code: 0, data: { list: [] } })),
  ]);

  renderLayout(`
    <h2 class="section-title">管理后台</h2>
    <div class="tabs" id="admin-tabs">
      <button data-tab="tags" class="active">标签</button>
      <button data-tab="canteens">食堂</button>
      <button data-tab="stalls">窗口</button>
      <button data-tab="reviews">评论</button>
      <button data-tab="users">用户</button>
      <div class="tab-indicator"></div>
    </div>
    <div id="admin-content" class="panel"></div>
  `);

  const adminTabs = {
    tags: {
      html: () => `
        <h3 class="section-title">标签管理</h3>
        <form id="create-tag" class="form-grid mb-md">
          <div class="form-row">
            <input name="name" placeholder="标签名称" required />
            <input name="description" placeholder="标签描述" />
            <button type="submit" class="btn-sm">新增</button>
          </div>
        </form>
        <div class="tag-list">${(tags.data.list || []).map(t =>
          `<span class="tag">${t.name}</span>`).join("")}</div>`,
      init: (c) => {
        c.querySelector("#create-tag").onsubmit = async (e) => {
          e.preventDefault();
          const d = Object.fromEntries(new FormData(e.currentTarget).entries());
          const r = await api.adminCreateTag(d);
          toast(r.code === 0 ? "标签已创建" : r.message, r.code === 0 ? "success" : "error");
          if (r.code === 0) await renderAdmin();
        };
      }
    },
    canteens: {
      html: () => `
        <h3 class="section-title">食堂管理</h3>
        <form id="create-canteen" class="form-grid mb-md">
          <div class="form-row">
            <input name="name" placeholder="食堂名称" required />
            <input name="location" placeholder="位置" />
            <input name="description" placeholder="描述" />
            <button type="submit" class="btn-sm">新增</button>
          </div>
        </form>
        <table class="admin-table">
          <thead><tr><th>ID</th><th>名称</th><th>位置</th><th>操作</th></tr></thead>
          <tbody>${(canteens.data.list || []).map(c => `
            <tr>
              <td>${c.id}</td><td>${c.name}</td><td>${c.location || "-"}</td>
              <td><button class="btn-sm btn-ghost" data-edit-canteen="${c.id}">编辑</button></td>
            </tr>`).join("")}</tbody>
        </table>`,
      init: (c) => {
        c.querySelector("#create-canteen").onsubmit = async (e) => {
          e.preventDefault();
          const d = Object.fromEntries(new FormData(e.currentTarget).entries());
          const r = await api.adminCreateCanteen(d);
          toast(r.code === 0 ? "食堂已创建" : r.message, r.code === 0 ? "success" : "error");
          if (r.code === 0) await renderAdmin();
        };
      }
    },
    stalls: {
      html: () => `
        <h3 class="section-title">窗口管理</h3>
        <form id="create-stall" class="form-grid mb-md">
          <div class="form-row">
            <select name="canteen_id" required>
              <option value="">选择食堂</option>
              ${(canteens.data.list || []).map(c => `<option value="${c.id}">${c.name}</option>`).join("")}
            </select>
            <input name="name" placeholder="窗口名称" required />
            <select name="category">${optionsHtml(categories.data.list || [], "选择分类")}</select>
          </div>
          <input name="description" placeholder="窗口描述" />
          <input name="tags" placeholder="标签（逗号分隔）" />
          <button type="submit" class="btn-sm">新增窗口</button>
        </form>
        <table class="admin-table">
          <thead><tr><th>ID</th><th>名称</th><th>食堂</th><th>分类</th><th>评分</th><th>状态</th><th>操作</th></tr></thead>
          <tbody>${(stalls.data.list || []).map(s => `
            <tr>
              <td>${s.id}</td><td>${s.name}</td><td>${s.canteen_name}</td><td>${s.category || "-"}</td>
              <td>${Number(s.avg_rating || 0).toFixed(1)}</td>
              <td><span class="status-badge ${s.status === 1 ? 'active' : 'disabled'}">${s.status === 1 ? '启用' : '停用'}</span></td>
              <td>
                <button class="btn-sm btn-ghost" data-disable-stall="${s.id}">${s.status === 1 ? '停用' : '已停用'}</button>
              </td>
            </tr>`).join("")}</tbody>
        </table>`,
      init: (c) => {
        c.querySelector("#create-stall").onsubmit = async (e) => {
          e.preventDefault();
          const d = Object.fromEntries(new FormData(e.currentTarget).entries());
          d.canteen_id = Number(d.canteen_id);
          if (d.tags) d.tags = d.tags.split(/[,，]/).map(t => t.trim()).filter(Boolean);
          const r = await api.adminCreateStall(d);
          toast(r.code === 0 ? "窗口已创建" : r.message, r.code === 0 ? "success" : "error");
          if (r.code === 0) await renderAdmin();
        };
        c.querySelectorAll("[data-disable-stall]").forEach(btn => {
          btn.onclick = async () => {
            const r = await api.adminDeleteStall(btn.dataset.disableStall);
            toast(r.code === 0 ? "窗口已停用" : r.message, r.code === 0 ? "success" : "error");
            if (r.code === 0) await renderAdmin();
          };
        });
      }
    },

    reviews: {
      html: () => `
        <h3 class="section-title">评论管理</h3>
        <form id="del-review" class="form-grid mb-md">
          <div class="form-row">
            <input name="review_id" placeholder="评论 ID" required />
            <button type="submit" class="btn-sm" style="background:var(--error);">删除评论</button>
          </div>
        </form>`,
      init: (c) => {
        c.querySelector("#del-review").onsubmit = async (e) => {
          e.preventDefault();
          const id = new FormData(e.currentTarget).get("review_id");
          const r = await api.adminDeleteReview(id);
          toast(r.code === 0 ? "评论已删除" : r.message, r.code === 0 ? "success" : "error");
        };
      }
    },
    users: {
      html: () => {
        const list = usersResult.data?.list || [];
        if (!list.length) return `<div class="empty">还没有用户数据</div>`;
        return `
          <h3 class="section-title">用户管理</h3>
          <table class="admin-table">
            <thead><tr><th>ID</th><th>学号</th><th>昵称</th><th>角色</th><th>操作</th></tr></thead>
            <tbody>${list.map(u => `
              <tr>
                <td>${u.id}</td><td>${u.student_id}</td><td>${u.username}</td>
                <td><span class="status-badge role-badge">${roleLabel(u.role)}</span></td>
                <td>
                  ${u.id !== state.user.id ? `
                    ${u.role === 0 ? `<button class="btn-sm btn-secondary" data-set-role="${u.id}" data-role="1">设为管理员</button>` : ""}
                    ${u.role >= 1 ? `<button class="btn-sm btn-ghost" data-set-role="${u.id}" data-role="0">设为普通用户</button>` : ""}
                  ` : `<span class="text-sm text-muted">当前账号</span>`}
                </td>
              </tr>`).join("")}</tbody>
          </table>`;
      },
      init: (c) => {
        c.querySelectorAll("[data-set-role]").forEach(btn => {
          btn.onclick = async () => {
            const r = await api.adminUpdateUserRole(btn.dataset.setRole, { role: Number(btn.dataset.role) });
            toast(r.code === 0 ? "权限已更新" : r.message, r.code === 0 ? "success" : "error");
            if (r.code === 0) await renderAdmin();
          };
        });
      }
    },
  };

  // Init admin tabs
  const tabBar = app.querySelector("#admin-tabs");
  const contentEl = app.querySelector("#admin-content");
  const tabBtns = tabBar.querySelectorAll("button[data-tab]");
  const tabInd = tabBar.querySelector(".tab-indicator");

  function switchTab(key) {
    tabBtns.forEach(b => b.classList.toggle("active", b.dataset.tab === key));
    const activeBtn = tabBar.querySelector(`button[data-tab="${key}"]`);
    if (activeBtn && tabInd) {
      tabInd.style.left = activeBtn.offsetLeft + "px";
      tabInd.style.width = activeBtn.offsetWidth + "px";
    }
    const cfg = adminTabs[key];
    if (cfg) {
      contentEl.innerHTML = typeof cfg.html === "function" ? cfg.html() : cfg.html;
      if (cfg.init) cfg.init(contentEl);
    }
  }

  tabBtns.forEach(b => { b.onclick = () => switchTab(b.dataset.tab); });
  requestAnimationFrame(() => switchTab("tags"));
}

/* ---- Router ---- */
async function router() {
  await ensureMe();
  const route = location.hash.slice(2) || "";
  if (route === "" || route === "/") return renderHome();
  if (route === "login") return renderLogin();
  if (route === "register") return renderRegister();
  if (route.startsWith("stall/")) return renderStall(route.split("/")[1]);
  if (route === "rankings") return renderRankings();
  if (route === "profile") return renderProfile();
  if (route === "admin") return renderAdmin();
  return renderHome();
}

window.addEventListener("hashchange", router);
router();
