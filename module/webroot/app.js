/*
 * The WebUI's logic.
 *
 * It does not parse a config, decide a rule or hold one: every read comes from
 * sync-tool (--status, --packages, --get-config) and every write goes back
 * through it (--set-config), so the page and the running policy cannot disagree
 * about which format a file is in. The tool is the one place that reads and
 * writes the config, and it validates before it writes. What is left here is the
 * shape of a phone screen over that: a home list, a searchable app list, a
 * per-app rule, and templates.
 *
 * The screens follow HMA-OSS's layout because these settings are already
 * familiar in that shape. Nothing is copied from it: it is AGPL-3.0 and Kotlin,
 * this is GPL-2.0 and a web page. What it has that a kernel-side uid guard
 * cannot honour -- icon/launcher hiding, per-hook switches, logs, accessibility
 * -- is left out rather than shown as a switch that would do nothing.
 */

"use strict";

const MODULE_DIR = "/data/adb/modules/hma-uidfake";
const TOOL = MODULE_DIR + "/sync-tool";
const THEME_KEY = "hma-uidfake.theme";
const REPO_URL = "https://github.com/Mobilelegends74/hma-uidfake";
const MANUAL_KMIS = ["android12-5.10", "android13-5.10", "android13-5.15", "android14-5.15", "android14-6.1", "android15-6.6", "android16-6.12", "android16-6.12-oneplus-6.12.38", "android17-6.18"];

/* ---- talking to sync-tool ---- */

/* KernelSU runs module WebUI JS and gives it `ksu`. exec runs as root and hands
 * back stdout; older and newer builds disagree on whether that is a bare string
 * or an {errno, stdout, stderr} object, so both are taken. */
function hasRoot() {
  return typeof ksu !== "undefined" && ksu && typeof ksu.exec === "function";
}

function execRaw(command) {
  return new Promise((resolve, reject) => {
    if (!hasRoot()) {
      reject(new Error("no ksu.exec: open this page from the KernelSU manager"));
      return;
    }
    let done = false;
    const finish = (data) => {
      if (done) return;
      done = true;
      if (typeof data === "string") {
        resolve({ errno: 0, stdout: data, stderr: "" });
        return;
      }
      if (data && typeof data === "object") {
        const errno = typeof data.errno === "number" ? data.errno : 0;
        resolve({
          errno: errno,
          stdout: typeof data.stdout === "string" ? data.stdout : "",
          stderr: typeof data.stderr === "string" ? data.stderr : "",
        });
        return;
      }
      resolve({
        errno: 0,
        stdout: String(data == null ? "" : data),
        stderr: "",
      });
    };
    try {
      const r = ksu.exec(command);
      if (r && typeof r.then === "function") r.then(finish, reject);
      else finish(r);
    } catch (e) {
      reject(e);
    }
  });
}

/* One sync-tool command whose stdout is a single JSON document. A non-zero exit
 * or a body that does not parse is raised, never half-read. */
async function runJson(command) {
  const r = await execRaw(command);
  const text = (r.stdout || "").trim();
  if (r.errno !== 0 && !text) {
    throw new Error((r.stderr || "").trim() || "sync-tool exited " + r.errno);
  }
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch (e) {
    throw new Error("sync-tool did not answer with JSON: " + firstLine(text));
  }
  return parsed;
}

async function runOk(command) {
  const r = await execRaw(command);
  if (r.errno !== 0) {
    throw new Error((r.stderr || "").trim() || "sync-tool exited " + r.errno);
  }
  return r.stdout;
}

function firstLine(text) {
  const line = (text || "").split("\n")[0] || "";
  return line.length > 160 ? line.slice(0, 157) + "..." : line;
}

const api = {
  inject: async (candidate) => {
    if (!MANUAL_KMIS.includes(candidate)) throw new Error("Invalid KMI");
    const result = await execRaw("sh " + MODULE_DIR + "/manual-inject.sh " + candidate);
    const output = [result.stdout, result.stderr].filter(Boolean).join("\n");
    return { ok: result.errno === 0 && output.includes("[LOADED]"), output };
  },
  setSource: (mode) => {
    if (mode !== "external" && mode !== "manual") return Promise.reject(new Error("Invalid source mode"));
    return runJson(TOOL + " --set-source " + mode);
  },
  diagnostics: () => runJson("sh " + MODULE_DIR + "/diagnostics.sh"),
  kernelVersion: () => runOk("cat /proc/version"),
  status: () => runJson(TOOL + " --status"),
  packages: () => runJson(TOOL + " --packages"),
  config: () => runJson(TOOL + " --get-config"),
  save: async (config) => {
    /* base64 keeps the document out of the shell's quoting: it is the one
     * alphabet that survives a pipe unchanged. */
    const b64 = btoa(unescape(encodeURIComponent(JSON.stringify(config))));
    // Avoid leaving a root-readable policy export in /data/local/tmp.
    // The base64 alphabet is shell-safe; sync-tool reads JSON from stdin.
    await runOk(
      "printf '%s' '" + b64 + "' | base64 -d | " + TOOL + " --set-config",
    );
  },
  version: async () => {
    const out = await runOk("sed -n 's/^version=//p' " + MODULE_DIR + "/module.prop");
    return (out || "").trim();
  },
};

/* ---- icons ---- */

/* Drawn here rather than taken from anywhere: four shapes are all this page
 * needs, and a font or a sprite would be more to ship. */
const ICONS = {
  back: "M15 5l-7 7 7 7",
  next: "M9 5l7 7-7 7",
  search: "M11 4a7 7 0 105 12A7 7 0 0011 4zm5 12l4 4",
  more: "M6 12h.01M12 12h.01M18 12h.01",
  shield: "M12 3l7 3v6c0 4-3 7-7 9-4-2-7-5-7-9V6z",
  apps: "M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z",
  layers: "M12 3l9 5-9 5-9-5zM3 13l9 5 9-5",
  gear: "M12 9a3 3 0 100 6 3 3 0 000-6zM4 12H2m20 0h-2M12 4V2m0 20v-2",
  info: "M12 3a9 9 0 100 18 9 9 0 000-18zm0 5v.01M12 11v6",
  save: "M12 3v12m0 0l-4-4m4 4l4-4M4 19h16",
  diagnostics: "M6 3h8l4 4v14H6zM14 3v5h4M8 15h2l1-3 2 6 1-3h2",
  restore: "M12 21V9m0 0L8 13m4-4l4 4M4 5h16",
  help: "M12 3a9 9 0 100 18 9 9 0 000-18zm0 13v.01M9.5 9.5a2.5 2.5 0 114 2c-.8.7-1.5 1-1.5 2",
  plus: "M12 5v14M5 12h14",
  trash: "M4 7h16M9 7V5h6v2m-8 0l1 13h8l1-13",
  edit: "M4 20h4L20 8l-4-4L4 16z",
  check: "M4 12l5 5L20 6",
  sun: "M12 7a5 5 0 100 10 5 5 0 000-10zM12 2v2m0 16v2M2 12h2m16 0h2M5 5l1 1m12 12l1 1M19 5l-1 1M6 18l-1 1",
  filter: "M4 6h16M7 12h10M10 18h4",
  code: "M9 8l-4 4 4 4M15 8l4 4-4 4",
};

function icon(name, cls) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "1.9");
  svg.setAttribute("stroke-linecap", "round");
  svg.setAttribute("stroke-linejoin", "round");
  if (cls) svg.setAttribute("class", cls);
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", ICONS[name] || "");
  svg.append(path);
  return svg;
}

/* ---- state ---- */

const state = {
  loaded: false,
  failed: null,
  status: null,
  packages: [],
  config: null,
  version: "",
  stack: [{ screen: "home", param: null }],
  /* list screen controls */
  query: "",
  filter: "all",
  sort: "package",
  reverse: false,
};

function screen() {
  return state.stack[state.stack.length - 1];
}

function go(name, param) {
  state.stack.push({ screen: name, param: param === undefined ? null : param });
  render();
  window.scrollTo(0, 0);
}

function back() {
  if (state.stack.length > 1) state.stack.pop();
  render();
  window.scrollTo(0, 0);
}

/* Jump back to a screen that is already below, so Home does not pile up. */
function goRoot(name) {
  state.stack = [{ screen: name, param: null }];
  render();
  window.scrollTo(0, 0);
}

/* ---- small helpers ---- */

function $(id) {
  return document.getElementById(id);
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text != null) node.textContent = text; /* English function labels remain unchanged */
  return node;
}

let snackTimer = null;
function toast(message, kind) {
  const box = $("snackbar");
  box.textContent = t(message);
  box.className = "snackbar" + (kind ? " " + kind : "");
  if (snackTimer) clearTimeout(snackTimer);
  snackTimer = setTimeout(() => box.classList.add("hidden"), 2800);
}

let busyCount = 0;
function busy(on) {
  busyCount += on ? 1 : -1;
  if (busyCount < 0) busyCount = 0;
  $("busy").classList.toggle("hidden", busyCount === 0);
}

/* Wrap a mutating action so every path gets the spinner and the snackbar. */
async function guard(label, fn) {
  busy(true);
  try {
    await fn();
  } catch (e) {
    toast(t(label) + ": " + t(e.message), "bad");
  } finally {
    busy(false);
  }
}

function pkgInfo(name) {
  for (const p of state.packages) if (p.name === name) return p;
  return null;
}

function templateCount(name) {
  const list = (state.config && state.config.templates) || {};
  return (list[name] || []).length;
}

function entryOf(caller) {
  const apps = (state.config && state.config.apps) || {};
  return apps[caller] || null;
}

/* How many apps a caller entry actually hides, templates counted through. */
function hiddenCount(caller) {
  const e = entryOf(caller);
  if (!e) return 0;
  const set = new Set(e.hide || []);
  for (const t of e.templates || []) {
    for (const p of (state.config.templates || {})[t] || []) set.add(p);
  }
  return set.size;
}

function entrySummary(caller) {
  const e = entryOf(caller);
  if (!e) return "";
  if (e.hide_all) return "hide everything";
  const n = hiddenCount(caller);
  const t = (e.templates || []).length;
  if (n === 0 && t === 0) return "nothing hidden yet";
  const bits = [];
  if (n) bits.push(n + " app" + (n === 1 ? "" : "s"));
  if (t) bits.push(t + " template" + (t === 1 ? "" : "s"));
  return bits.join(" + ");
}

function avatar(text) {
  const box = el("div", "lead");
  const name = (text || "?").split(".").pop() || "?";
  box.textContent = name.slice(0, 1).toUpperCase();
  return box;
}

function switch_(checked, onChange) {
  const label = el("label", "switch");
  const input = document.createElement("input");
  input.type = "checkbox";
  input.checked = !!checked;
  input.addEventListener("change", () => onChange(input.checked));
  /* A switch often sits on a row that is itself tappable. Flipping one must
   * not also open the screen behind it, so the tap stops here. */
  label.addEventListener("click", (ev) => ev.stopPropagation());
  label.append(input, document.createElement("i"));
  return label;
}

function chips(values, active, onPick) {
  const row = el("div", "chiprow");
  for (const v of values) {
    const b = el("button", "chip", v.label);
    b.type = "button";
    b.setAttribute("aria-pressed", String(v.id === active));
    b.addEventListener("click", () => onPick(v.id));
    row.append(b);
  }
  return row;
}

function searchBar(value, placeholder, onInput) {
  const box = el("div", "search");
  box.append(icon("search"));
  const input = document.createElement("input");
  input.type = "search";
  input.value = value || "";
  input.placeholder = placeholder;
  input.addEventListener("input", () => onInput(input.value));
  box.append(input);
  return box;
}

/* A row is a div rather than a button because a row can carry a switch, and an
 * input inside a button is neither valid nor reliable: the browser would send
 * the label's tap to the button as well. Keyboard access is added by hand
 * instead, so a tappable row still answers to Enter and Space. */
function row(opts) {
  const node = el("div", opts.onClick ? "row clickable" : "row");
  if (opts.onClick) {
    node.setAttribute("role", "button");
    node.setAttribute("tabindex", "0");
    node.addEventListener("click", opts.onClick);
    node.addEventListener("keydown", (ev) => {
      if (ev.key === "Enter" || ev.key === " ") {
        ev.preventDefault();
        opts.onClick();
      }
    });
  }
  if (opts.lead) node.append(opts.lead);
  const main = el("div", "main");
  const title = el("div", "title");
  if (Array.isArray(opts.title)) title.append(...opts.title);
  else title.textContent = opts.title || "";
  main.append(title);
  if (opts.sub) {
    const sub = el("div", "sub" + (opts.subMono ? " mono" : ""), opts.sub);
    main.append(sub);
  }
  node.append(main);
  if (opts.trail) {
    const trail = el("div", "trail");
    trail.append(...(Array.isArray(opts.trail) ? opts.trail : [opts.trail]));
    node.append(trail);
  }
  return node;
}

function card(children) {
  const box = el("div", "card");
  const list = el("div", "list");
  list.append(...children);
  box.append(list);
  return box;
}

function note(text) {
  return el("p", "note", text);
}

/* ---- theme ---- */

function applyTheme() {
  let theme = "system";
  try {
    theme = localStorage.getItem(THEME_KEY) || "system";
  } catch (e) {
    /* a WebView with storage off still gets the system theme */
  }
  if (theme === "system") document.documentElement.removeAttribute("data-theme");
  else document.documentElement.setAttribute("data-theme", theme);
  return theme;
}

function setTheme(theme) {
  try {
    localStorage.setItem(THEME_KEY, theme);
  } catch (e) {
    /* not fatal: the choice just will not survive a reload */
  }
  applyTheme();
  render();
}

/* ---- writing the config ---- */

/* Write through on every change: the tool validates, keeps one .bak and pushes
 * to the kernel in the same step, so what is on screen is what the kernel has. */
async function persist() {
  // Saving manual edits never changes the explicitly selected source.
  if ((!state.status || state.status.rule_mode !== "manual") && !window.confirm(languageText(
    "Ручные правила будут сохранены, но не активированы. Сейчас используется HMA/HMA-OSS. Для применения отдельно выберите ручной режим на главном экране. Продолжить?",
    "Manual rules will be saved but not activated. HMA/HMA-OSS is selected. To apply them, explicitly select Manual on the home screen. Continue?"
  ))) {
    throw new Error("Independent policy change cancelled");
  }
  await api.save(state.config);
  state.status = await api.status();
}

/* Mutate a copy, write it, and keep it only if the tool took it: a refused
 * write must not leave the page showing a config the kernel never got. */
async function change(label, mutate) {
  const before = JSON.parse(JSON.stringify(state.config));
  mutate(state.config);
  await guard(label, async () => {
    try {
      await persist();
      toast("Saved");
    } catch (e) {
      /* Put the old config back so the screen and the file agree again. */
      state.config = before;
      throw e;
    }
  });
  /* Repaint on both paths: after a refused write a switch would otherwise sit
   * in the position the tap put it, which is not the position the file has. */
  render();
}

/* ---- loading ---- */

async function load() {
  try {
    const [status, packages, config] = await Promise.all([
      api.status(),
      api.packages(),
      api.config(),
    ]);
    state.status = status;
    state.packages = packages.ok ? packages.packages || [] : [];
    state.config = normalize(config.config || {});
    state.failed = packages.ok ? null : t("install: package list unavailable");
    api.version().then((v) => {
      state.version = v;
      /* The version arrives after the first paint; the two screens that show it
       * are repainted so it is not left reading "unknown". */
      const now = screen().screen;
      if (now === "about" || now === "home") render();
    });
  } catch (e) {
    state.failed = e.message;
  }
  state.loaded = true;
  render();
}

function normalize(config) {
  const out = config && typeof config === "object" ? config : {};
  if (!out.version) out.version = 2;
  if (!out.mode) out.mode = "blacklist";
  if (!out.hide_system) out.hide_system = false;
  if (!out.templates || typeof out.templates !== "object") out.templates = {};
  if (!out.apps || typeof out.apps !== "object") out.apps = {};
  return out;
}

/* ---- rendering ---- */

function render() {
  const app = $("app");
  app.replaceChildren();

  const s = screen();
  const bar = el("header", "appbar");
  const isHome = s.screen === "home";
  if (!isHome) {
    const b = el("button", "icon-btn");
    b.type = "button";
    b.title = t("Back");
    b.append(icon("back"));
    b.addEventListener("click", back);
    bar.append(b);
  }
  bar.append(el("h1", isHome ? "lead-title" : null, titleFor(s)));
  const actions = actionsFor(s);
  for (const a of actions) {
    const b = el("button", "icon-btn");
    b.type = "button";
    b.title = t(a.title);
    b.append(icon(a.icon));
    b.addEventListener("click", a.onClick);
    bar.append(b);
  }
  app.append(bar);

  const body = el("main", "screen");
  app.append(body);

  if (!state.loaded) {
    body.append(el("div", "empty", "Loading..."));
    return;
  }
  if (state.failed && !state.config) {
    body.append(el("div", "empty", state.failed));
    return;
  }

  const view = {
    home: viewHome,
    apps: viewApps,
    app: viewApp,
    templates: viewTemplates,
    template: viewTemplate,
    picker: viewPicker,
    settings: viewSettings,
    about: viewAbout,
    raw: viewRaw,
    backup: viewBackup,
    help: viewHelp,
  }[s.screen];

  if (view) view(body, s.param);
}

function titleFor(s) {
  switch (s.screen) {
    case "home":
      return "HMA UID Fake fork";
    case "apps":
      return "Manage apps";
    case "app":
      return "App rule";
    case "templates":
      return "Templates";
    case "template":
      return s.param || "Template";
    case "picker":
      return s.param && s.param.title ? s.param.title : "Select apps";
    case "settings":
      return "Settings";
    case "about":
      return "About";
    case "raw":
      return "Raw config";
    case "backup":
      return "Backup & restore";
    case "help":
      return "How it works";
    default:
      return "HMA UID Fake fork";
  }
}

function actionsFor(s) {
  const actions = s.screen === "home"
    ? [{ title: "Settings", icon: "gear", onClick: () => go("settings") }]
    : [];
  // All menu screens have their own explanation; this never changes policy.
  actions.push({
    title: languageText("Справка по разделу", "About this screen"),
    icon: "help",
    onClick: () => showContextHelp(screenHelpKey(s)),
  });
  return actions;
}

function languageText(ru, en) {
  return HMA_UI_LOCALE === "ru" ? ru : en;
}

function screenHelpKey(s) {
  if (s.screen === "picker" && s.param && s.param.names) return "pickerTemplates";
  return s.screen;
}

function helpButton(key) {
  const btn = el("button", "icon-btn help-inline");
  btn.type = "button";
  btn.title = languageText("Что делает этот раздел", "What does this do?");
  btn.setAttribute("aria-label", btn.title);
  btn.append(icon("help"));
  btn.addEventListener("click", (event) => {
    event.stopPropagation(); // do not activate the parent navigational row
    showContextHelp(key);
  });
  btn.addEventListener("keydown", (event) => event.stopPropagation());
  return btn;
}

function showContextHelp(key) {
  const item = window.HMA_CONTEXT_HELP && window.HMA_CONTEXT_HELP[key];
  if (!item) return;
  const locale = HMA_UI_LOCALE === "ru" ? "ru" : "en";
  const data = item[locale];
  sheet({
    title: data.title,
    infoOnly: true,
    confirm: languageText("Понятно", "Got it"),
    body: (wrap) => {
      wrap.className = "context-help";
      wrap.append(el("p", "context-help-intro", data.intro));
      for (const section of data.sections) {
        wrap.append(el("h4", "context-help-heading", section.title));
        wrap.append(el("p", "context-help-paragraph", section.text));
      }
    },
    onConfirm: () => false,
  });
}

/* ---- home ---- */

function viewHome(body) {
  const hero = el("div", "hero");
  hero.append(el("h2", null, "HMA UID Fake fork"));
  hero.append(
    el("p", null, languageText(
      "Защита UID на уровне ядра: выбранные приложения выглядят отсутствующими при системных проверках UID. Работает вместе с HMA/HMA-OSS.",
      "Kernel-side UID guard. Selected apps appear absent to UID checks. Works alongside HMA/HMA-OSS."
    )),
  );
  body.append(hero);

  body.append(statusCard());
  body.append(sourceModeCard());
  // Unknown/failed WebUI status is not proof that loading failed. Only show
  // recovery when the backend explicitly reports no kernel endpoint.
  if (state.status && state.status.kernel === null) {
    body.append(card([row({
      lead: leadIcon("shield"),
      title: "Manual inject",
      sub: manualInjectRunning
        ? languageText("Попытка загрузки…", "Attempting load…")
        : languageText("Выбрать .ko · без проверки KMI/ABI модулем", "Choose a .ko · skip userspace KMI/ABI checks"),
      trail: icon("next"),
      onClick: showManualInject,
    })]));
  }

  const rules = Object.keys(state.config.apps || {}).length;
  const templates = Object.keys(state.config.templates || {}).length;

  body.append(
    card([
      row({
        lead: leadIcon("apps"),
        title: "Manage apps",
        sub: state.status && state.status.rule_mode !== "manual"
          ? languageText("Сохранённые ручные правила · не активны", "Saved manual rules · inactive")
          : rules ? rules + " app" + (rules === 1 ? "" : "s") + " with a rule" : "no rules yet",
        trail: [helpButton("apps"), icon("next")],
        onClick: () => go("apps"),
      }),
      row({
        lead: leadIcon("layers"),
        title: "Templates",
        sub: templates
          ? templates + " template" + (templates === 1 ? "" : "s")
          : "no templates yet",
        trail: [helpButton("templates"), icon("next")],
        onClick: () => go("templates"),
      }),
    ]),
  );

  body.append(
    card([
      row({
        lead: leadIcon("save"),
        title: "Backup & restore",
        sub: "copy the config, paste one back",
        trail: [helpButton("backup"), icon("next")],
        onClick: () => go("backup"),
      }),
      row({
        lead: leadIcon("help"),
        title: "How it works",
        sub: "what this does and does not cover",
        trail: [helpButton("help"), icon("next")],
        onClick: () => go("help"),
      }),
      row({
        lead: leadIcon("gear"),
        title: "Settings",
        sub: "theme, raw config",
        trail: [helpButton("settings"), icon("next")],
        onClick: () => go("settings"),
      }),
      row({
        lead: leadIcon("info"),
        title: "About",
        sub: state.version ? "version " + state.version : "module information",
        trail: [helpButton("about"), icon("next")],
        onClick: () => go("about"),
      }),
      row({
        lead: leadIcon("diagnostics"),
        title: languageText("Сохранить диагностику", "Save diagnostics"),
        sub: diagnosticsRunning
          ? languageText("Сбор архива… Пожалуйста, подождите", "Collecting archive… Please wait")
          : languageText("Журналы и правила в одном архиве", "Logs and rules in one archive"),
        trail: icon("next"),
        onClick: saveDiagnostics,
      }),
    ]),
  );
}

let manualInjectRunning = false;
let manualChoicesLoading = false;
function manualKernelChoices(version) {
  // Match the running Linux branch, not the Android userspace version.
  const match = /^Linux version (\d+\.\d+)(?:\.|[-\s]|$)/.exec(String(version).trim());
  return match ? MANUAL_KMIS.filter((candidate) => candidate.split("-")[1] === match[1]) : [];
}

async function showManualInject() {
  if (manualInjectRunning || manualChoicesLoading) return;
  manualChoicesLoading = true;
  let choices = [];
  let error = "";
  try {
    choices = manualKernelChoices(await api.kernelVersion());
  } catch (err) {
    error = firstLine(err.message);
  } finally {
    manualChoicesLoading = false;
  }
  sheet({
    title: "Manual inject",
    text: languageText(
      "Выбранный .ko будет передан ядру БЕЗ наших проверок KMI/ABI, даже при ранее обнаруженных несовпадениях. Риск зависания, перезагрузки и потери несохранённых данных. Это не прошивка ядра. Само ядро может отказать в загрузке; его защита не отключается. Автоматический режим и правила HMA не меняются.",
      "The selected .ko goes to the kernel WITHOUT our KMI/ABI checks, even after known mismatches. Risk of hangs, reboots and loss of unsaved data. This does not flash a kernel. The kernel may still reject loading; its protections are not disabled. Automatic mode and HMA rules are unchanged."
    ),
    infoOnly: true,
    confirm: languageText("Закрыть", "Close"),
    body: (wrap) => {
      if (!choices.length) {
        wrap.append(note(error || languageText("Для текущей ветки ядра нет вариантов в этом пакете либо не удалось определить ветку ядра.", "No packaged variants for the running kernel branch, or the branch could not be identified.")));
        return;
      }
      const grid = el("div", "manual-kmi-grid");
      for (const candidate of choices) {
        const button = el("button", "btn tonal manual-kmi-choice");
        button.append(el("span", "manual-kmi-name", candidate.split("-").slice(0, 2).join("-")));
        if (candidate === "android16-6.12-oneplus-6.12.38") {
          button.append(el("span", "manual-kmi-detail", "OnePlus 6.12.38"));
        }
        button.type = "button";
        button.addEventListener("click", () => confirmManualInject(candidate));
        grid.append(button);
      }
      wrap.append(grid);
    },
    onConfirm: () => false,
  });
}

function confirmManualInject(candidate) {
  if (manualInjectRunning || !MANUAL_KMIS.includes(candidate)) return;
  sheet({
    title: "Manual inject · " + candidate,
    text: languageText("Вы сознательно запускаете загрузку без наших проверок совместимости. Неподходящий .ko может вызвать зависание или перезагрузку. Ядро всё ещё вправе отклонить его. Продолжить на свой риск?", "You are explicitly loading without our compatibility checks. An incompatible .ko may hang or reboot the device. The kernel may still reject it. Continue at your own risk?"),
    confirm: languageText("Принимаю риск — загрузить", "Accept risk — load"),
    onConfirm: async () => {
      if (manualInjectRunning) return true;
      closeSheet();
      manualInjectRunning = true;
      render();
      let message;
      try {
        // Refresh before injection too: another process may have loaded it.
        state.status = await api.status();
        if (!state.status || state.status.kernel !== null) throw new Error(languageText("Модуль уже загружен или статус не подтверждён. Обновите страницу.", "Module already loaded or status unconfirmed. Refresh the page."));
        const result = await api.inject(candidate);
        message = (result.ok ? languageText("Ядро приняло модуль. Синхронизация запускается.\n", "Kernel accepted the module. Synchronization is starting.\n") : languageText("Модуль не загружен. Причина:\n", "Module not loaded. Reason:\n")) + result.output;
        state.status = await api.status();
      } catch (error) {
        message = error.message;
      } finally {
        manualInjectRunning = false;
        render();
      }
      sheet({ title: "Manual inject", text: message, infoOnly: true,
        confirm: languageText("Закрыть", "Close"), onConfirm: () => false });
      return true;
    },
  });
}

let sourceSwitching = false;
function sourceModeCard() {
  const box = el("div", "card pad");
  box.append(el("div", "section-title", languageText("Источник правил", "Rule source")));
  const mode = state.status && state.status.rule_mode;
  const seg = el("div", "segmented");
  for (const item of [
    { id: "external", label: "HMA / HMA-OSS" },
    { id: "manual", label: languageText("Ручной", "Manual") },
  ]) {
    const button = el("button", null, item.label);
    button.type = "button";
    button.disabled = sourceSwitching;
    button.setAttribute("aria-pressed", String(mode === item.id));
    button.addEventListener("click", () => switchRuleSource(item.id));
    seg.append(button);
  }
  box.append(seg);
  const active = state.status && state.status.active_config;
  box.append(note(mode === "manual"
    ? languageText("Используются только ручные правила из Manage apps. Правила HMA-OSS в этом режиме не добавляются.", "Only manual Manage apps rules are used. HMA-OSS rules are not merged in this mode.")
    : languageText("Автоматическая синхронизация HMA-OSS (или HMA). Сохранённые ручные правила не влияют на скрытие.", "Automatically follows HMA-OSS (or HMA). Saved manual rules do not affect hiding.")));
  if (!active) box.append(note(languageText("Ожидание конфига выбранного источника. Старая UID-политика очищается.", "Waiting for the selected source config. Previous UID policy is cleared.")));
  return box;
}

async function switchRuleSource(mode) {
  if (sourceSwitching || (state.status && state.status.rule_mode === mode)) return;
  if (!window.confirm(languageText(
    "Сменить источник правил? Текущая политика будет заменена. Сохранённые конфиги не удаляются.",
    "Switch rule source? The current policy will be replaced. Saved configs are not deleted."
  ))) return;
  sourceSwitching = true;
  render();
  await guard(languageText("Источник правил", "Rule source"), async () => {
    state.status = await api.setSource(mode);
    toast("Saved");
  });
  sourceSwitching = false;
  render();
}

let diagnosticsRunning = false;
function saveDiagnostics() {
  if (diagnosticsRunning) {
    toast(languageText("Диагностика уже собирается", "Diagnostics are already being collected"));
    return;
  }
  sheet({
    title: languageText("Сохранить диагностику", "Save diagnostics"),
    text: languageText(
      "Архив будет сохранён в Download. Он содержит журналы ядра, сведения об устройстве, названия приложений и правила UIDFAKE/HMA. Передавайте его только тому, кому доверяете. Настройки не изменяются. Сбор может занять до нескольких минут; не закрывайте WebUI.",
      "The archive will be saved to Download. It contains kernel logs, device details, app names and UIDFAKE/HMA rules. Share it only with someone you trust. Settings are not changed. Collection may take a few minutes; keep WebUI open."
    ),
    confirm: languageText("Сохранить архив", "Save archive"),
    onConfirm: async () => {
      // Close consent before collection; a later result sheet must not be
      // closed by the generic sheet handler when this promise completes.
      if (diagnosticsRunning) return true;
      closeSheet();
      diagnosticsRunning = true;
      render();
      await guard(languageText("Диагностика", "Diagnostics"), async () => {
        const result = await api.diagnostics();
        if (!result || !result.ok || typeof result.path !== "string" ||
            !/^\/storage\/emulated\/0\/Download\/uidfake-diagnostics-[A-Za-z0-9-]+\.tar\.gz$/.test(result.path)) {
          throw new Error(languageText("Архив не создан. Проверьте доступ к Download и свободное место.", "Archive was not created. Check Download access and free space."));
        }
        sheet({
          title: languageText("Диагностика сохранена", "Diagnostics saved"),
          text: languageText("Отправьте этот архив разработчику. Автоматической отправки нет.", "Send this archive to the developer. Nothing is uploaded automatically."),
          infoOnly: true,
          confirm: languageText("Готово", "Done"),
          body: (wrap) => {
            const path = el("textarea", "field");
            path.readOnly = true;
            path.value = result.path;
            path.style.minHeight = "90px";
            const copy = el("button", "btn tonal", languageText("Копировать путь", "Copy path"));
            copy.type = "button";
            copy.addEventListener("click", async () => {
              try {
                if (navigator.clipboard) await navigator.clipboard.writeText(result.path);
                else { path.select(); if (!document.execCommand("copy")) throw new Error("copy"); }
                toast("Copied");
              } catch (_) { toast("Copy failed", "bad"); }
            });
            wrap.append(path, copy);
          },
          onConfirm: () => false,
        });
      });
      diagnosticsRunning = false;
      render();
      return true;
    },
  });
}

function leadIcon(name) {
  const box = el("div", "lead");
  box.append(icon(name));
  return box;
}

function statusCard() {
  const box = el("div", "card pad");
  const st = state.status;
  if (!st) {
    box.append(el("div", "empty", "Kernel status unavailable"));
    return box;
  }

  const head = el("div", "row");
  head.style.padding = "0 0 6px";
  const lead = el("div", "lead");
  lead.append(icon("shield"));
  head.append(lead, el("div", "main"));
  const main = head.querySelector(".main");
  main.append(el("div", "title", "Kernel module"));
  main.append(
    el(
      "div",
      "sub",
      st.kernel ? "answering over kaux" : st.kernel_note || "not answering",
    ),
  );
  box.append(head);

  const kv = (k, v) => {
    const line = el("div", "kv");
    line.append(el("span", "k", k), el("span", "v", v));
    return line;
  };

  box.append(kv("Rules active in kernel", st.rules == null ? "unavailable" : String(st.rules)));
  box.append(kv("Rule source", st.source === "none" ? "none (idle)" : st.source));
  if (st.kernel && st.rules === 0) box.append(note(languageText("Модуль загружен, но активных правил нет. Проверьте выбранный источник.", "Module loaded, but no active rules. Check the selected rule source.")));
  if (st.kernel) {
    box.append(
      kv("Syscall hooks", st.kernel.native + " " + t("native") + ", " + st.kernel.compat + " " + t("compat")),
    );
    box.append(kv("APK inodes", String(st.kernel.apk_inodes)));
    if (st.kernel.summary) box.append(kv("Summary", st.kernel.summary));
  }
  box.append(kv("Config", st.active_config || languageText("Конфиг выбранного источника отсутствует", "Selected source config is absent")));
  return box;
}

/* ---- app list ---- */

function filteredPackages() {
  const q = state.query.trim().toLowerCase();
  let list = state.packages.slice();
  if (state.filter === "rules") list = list.filter((p) => entryOf(p.name));
  else if (state.filter === "system") list = list.filter((p) => p.system);
  else if (state.filter === "apps") list = list.filter((p) => !p.system);
  if (q) {
    list = list.filter(
      (p) => p.name.toLowerCase().includes(q) || String(p.uid).includes(q),
    );
  }
  const key = state.sort;
  list.sort((a, b) => (a[key] > b[key] ? 1 : a[key] < b[key] ? -1 : 0));
  if (state.reverse) list.reverse();
  return list;
}

function viewApps(body) {
  body.append(
    searchBar(state.query, "Search package or uid", (v) => {
      state.query = v;
      renderList();
    }),
  );

  body.append(
    chips(
      [
        { id: "all", label: "All" },
        { id: "rules", label: "With rule" },
        { id: "apps", label: "User" },
        { id: "system", label: "System" },
      ],
      state.filter,
      (id) => {
        state.filter = id;
        render();
      },
    ),
  );

  body.append(el("div", "list", null));
  renderList();
}

/* Only the list is rebuilt while typing, so the field keeps its focus. */
function renderList() {
  const holder = document.querySelector(".list");
  if (!holder) return;
  holder.replaceChildren();

  const list = filteredPackages();
  if (!state.packages.length) {
    holder.append(el("div", "empty", "No packages. Is the module installed?"));
    return;
  }
  if (!list.length) {
    holder.append(el("div", "empty", "Nothing matches"));
    return;
  }

  const frag = document.createDocumentFragment();
  for (const p of list) {
    const rule = entryOf(p.name);
    const title = [document.createTextNode(p.name)];
    if (rule) title.push(el("span", "badge", " rule"));
    frag.append(
      row({
        lead: avatar(p.name),
        title: title,
        sub: "uid " + p.uid + (p.system ? " · system" : "") + (rule ? " · " + entrySummary(p.name) : ""),
        subMono: true,
        trail: [
          switch_(!!rule, (on) => toggleRule(p.name, on)),
        ],
        onClick: () => go("app", p.name),
      }),
    );
  }
  holder.append(frag);
}

async function toggleRule(pkg, on) {
  if (on) {
    await change("Add rule", (c) => {
      if (!c.apps[pkg]) c.apps[pkg] = { hide: [] };
    });
    /* A rule that hides nothing does nothing, so open it right away -- but a
     * refused write has just been rolled back, and walking into a screen for a
     * rule the file does not have is worse than staying put. */
    if (entryOf(pkg)) go("app", pkg);
  } else {
    await change("Remove rule", (c) => {
      delete c.apps[pkg];
    });
  }
}

/* ---- one app ---- */

function viewApp(body, pkg) {
  const info = pkgInfo(pkg);
  const entry = entryOf(pkg);

  const head = el("div", "card pad");
  const hr = el("div", "row");
  hr.style.padding = "0";
  hr.append(avatar(pkg), el("div", "main"));
  const hm = hr.querySelector(".main");
  hm.append(el("div", "title mono", pkg));
  hm.append(
    el(
      "div",
      "sub",
      info
        ? "uid " + info.uid + (info.system ? " · system" : " · user") + " app"
        : "not in the package list",
    ),
  );
  head.append(hr);
  if (info && info.code_dir) head.append(el("div", "sub mono", info.code_dir));
  body.append(head);

  if (!entry) {
    body.append(el("div", "empty", "No rule for this app."));
    const bar = el("div", "actionbar");
    const add = el("button", "btn", "Add rule");
    add.type = "button";
    add.addEventListener("click", async () => {
      await change("Add rule", (c) => {
        c.apps[pkg] = { hide: [] };
      });
    });
    bar.append(add);
    body.append(bar);
    return;
  }

  /* mode */
  const modeCard = el("div", "card pad");
  modeCard.append(el("div", "section-title", "Mode"));
  const seg = el("div", "segmented");
  for (const m of [
    { id: "default", label: "Default" },
    { id: "blacklist", label: "Blacklist" },
    { id: "whitelist", label: "Whitelist" },
  ]) {
    const b = el("button", null, m.label);
    b.type = "button";
    b.setAttribute("aria-pressed", String((entry.mode || "default") === m.id));
    b.addEventListener("click", () =>
      change("Mode", (c) => {
        if (m.id === "default") delete c.apps[pkg].mode;
        else c.apps[pkg].mode = m.id;
      }),
    );
    seg.append(b);
  }
  modeCard.append(seg);
  modeCard.append(
    note(
      t("Default follows the module-wide mode (") +
        t(state.config.mode === "blacklist" ? "Blacklist" : "Whitelist") +
        t("). Blacklist hides what is listed; whitelist hides everything except what is listed."),
    ),
  );
  body.append(modeCard);

  /* switches */
  body.append(
    card([
      row({
        lead: leadIcon("shield"),
        title: "Hide everything",
        sub: "this app sees no other app at all",
        trail: [
          switch_(!!entry.hide_all, (on) =>
            change("Hide everything", (c) => {
              if (on) c.apps[pkg].hide_all = true;
              else delete c.apps[pkg].hide_all;
            }),
          ),
        ],
      }),
      row({
        lead: leadIcon("filter"),
        title: "Include system apps",
        sub: "hide system apps from it too",
        trail: [
          switch_(!!entry.hide_system, (on) =>
            change("Include system apps", (c) => {
              if (on) c.apps[pkg].hide_system = true;
              else delete c.apps[pkg].hide_system;
            }),
          ),
        ],
      }),
    ]),
  );

  /* hidden list + templates */
  body.append(
    card([
      row({
        lead: leadIcon("apps"),
        title: "Hidden apps",
        sub: (entry.hide || []).length
          ? (entry.hide || []).length + " listed"
          : "none listed",
        trail: [icon("next")],
        onClick: () =>
          go("picker", {
            title: "Hidden apps",
            selected: entry.hide || [],
            onDone: async (picked) => {
              await change("Hidden apps", (c) => {
                if (picked.length) c.apps[pkg].hide = picked;
                else delete c.apps[pkg].hide;
              });
              back();
            },
          }),
      }),
      row({
        lead: leadIcon("layers"),
        title: "Templates",
        sub: (entry.templates || []).length
          ? (entry.templates || []).join(", ")
          : "none",
        trail: [icon("next")],
        onClick: () =>
          go("picker", {
            title: "Templates",
            names: Object.keys(state.config.templates || {}),
            selected: entry.templates || [],
            onDone: async (picked) => {
              await change("Templates", (c) => {
                if (picked.length) c.apps[pkg].templates = picked;
                else delete c.apps[pkg].templates;
              });
              back();
            },
          }),
      }),
    ]),
  );

  const bar = el("div", "actionbar");
  const del = el("button", "btn danger", "Remove rule");
  del.type = "button";
  del.addEventListener("click", async () => {
    await change("Remove rule", (c) => {
      delete c.apps[pkg];
    });
    back();
  });
  bar.append(del);
  body.append(bar);
}

/* ---- templates ---- */

function viewTemplates(body) {
  const names = Object.keys(state.config.templates || {}).sort();
  if (!names.length) {
    body.append(
      el("div", "empty", "No templates yet. A template is a named list of apps you can give to many rules."),
    );
  } else {
    body.append(
      card(
        names.map((name) =>
          row({
            lead: leadIcon("layers"),
            title: name,
            sub:
              templateCount(name) + " app" + (templateCount(name) === 1 ? "" : "s"),
            trail: [icon("next")],
            onClick: () => go("template", name),
          }),
        ),
      ),
    );
  }

  const bar = el("div", "actionbar");
  const add = el("button", "btn", "New template");
  add.type = "button";
  add.addEventListener("click", () => newTemplate());
  bar.append(add);
  body.append(bar);
}

function newTemplate() {
  sheet({
    title: "New template",
    body: (wrap) => {
      const input = el("input", "field");
      input.placeholder = t("name, e.g. banking");
      wrap.append(input);
      setTimeout(() => input.focus(), 50);
      return () => input.value.trim();
    },
    confirm: "Create",
    onConfirm: async (readName) => {
      const name = readName();
      if (!name) return true;
      if (state.config.templates[name]) {
        toast("A template with that name already exists", "bad");
        return true;
      }
      await change("New template", (c) => {
        c.templates[name] = [];
        c.apps = c.apps || {};
      });
      go("template", name);
      return false;
    },
  });
}

function viewTemplate(body, name) {
  const members = state.config.templates[name] || [];
  if (!state.config.templates[name]) {
    body.append(el("div", "empty", "That template is gone."));
    return;
  }

  const head = el("div", "card pad");
  head.append(el("div", "section-title", "Name"));
  const input = el("input", "field");
  input.value = name;
  input.addEventListener("change", async () => {
    const next = input.value.trim();
    if (!next || next === name) {
      input.value = name;
      return;
    }
    if (state.config.templates[next]) {
      toast("A template with that name already exists", "bad");
      input.value = name;
      return;
    }
    await change("Rename", (c) => {
      c.templates[next] = c.templates[name];
      delete c.templates[name];
      /* Rules point at templates by name, so a rename has to follow through or
       * they would be left pointing at nothing. */
      for (const caller of Object.keys(c.apps || {})) {
        const list = c.apps[caller].templates;
        if (Array.isArray(list) && list.includes(name)) {
          c.apps[caller].templates = list.map((t) => (t === name ? next : t));
        }
      }
    });
    state.stack[state.stack.length - 1].param = next;
    render();
  });
  head.append(input);
  body.append(head);

  body.append(
    card([
      row({
        lead: leadIcon("apps"),
        title: "Members",
        sub: members.length
          ? members.length + " app" + (members.length === 1 ? "" : "s")
          : "none yet",
        trail: [icon("next")],
        onClick: () =>
          go("picker", {
            title: "Members",
            selected: members,
            onDone: async (picked) => {
              await change("Members", (c) => {
                c.templates[name] = picked;
              });
              back();
            },
          }),
      }),
    ]),
  );

  if (members.length) {
    body.append(
      card(
        members.map((p) =>
          row({
            lead: avatar(p),
            title: p,
            subMono: true,
            sub: pkgInfo(p) ? "uid " + pkgInfo(p).uid : "",
            trail: [icon("next")],
            onClick: () => go("app", p),
          }),
        ),
      ),
    );
  }

  const bar = el("div", "actionbar");
  const del = el("button", "btn danger", "Delete template");
  del.type = "button";
  del.addEventListener("click", async () => {
    await change("Delete template", (c) => {
      delete c.templates[name];
      for (const caller of Object.keys(c.apps || {})) {
        const list = c.apps[caller].templates;
        if (Array.isArray(list)) {
          const kept = list.filter((t) => t !== name);
          if (kept.length) c.apps[caller].templates = kept;
          else delete c.apps[caller].templates;
        }
      }
    });
    back();
  });
  bar.append(del);
  body.append(bar);
}

/* ---- picker ---- */

/* One screen serves both "which apps does this rule hide" and "who is in this
 * template": the second picks names, the first picks packages. */
function viewPicker(body, param) {
  const selected = new Set(param.selected || []);
  const byName = !!param.names;
  const items = byName
    ? param.names.slice().sort().map((n) => ({ name: n, uid: null, system: false }))
    : state.packages;

  const query = el("div");
  body.append(
    searchBar("", byName ? "Search template" : "Search package or uid", (v) => {
      paint(v);
    }),
  );

  const holder = el("div", "list");
  body.append(holder);

  function paint(q) {
    const needle = (q || "").trim().toLowerCase();
    const list = items.filter(
      (p) =>
        !needle ||
        p.name.toLowerCase().includes(needle) ||
        String(p.uid || "").includes(needle),
    );
    holder.replaceChildren();
    if (!list.length) {
      holder.append(el("div", "empty", "Nothing matches"));
      return;
    }
    const frag = document.createDocumentFragment();
    for (const p of list) {
      frag.append(
        row({
          lead: byName ? leadIcon("layers") : avatar(p.name),
          title: p.name,
          sub: p.uid == null ? "" : "uid " + p.uid + (p.system ? " · system" : ""),
          subMono: true,
          trail: [
            switch_(selected.has(p.name), (on) => {
              if (on) selected.add(p.name);
              else selected.delete(p.name);
              count.textContent = t(selected.size + " selected");
            }),
          ],
        }),
      );
    }
    holder.append(frag);
  }

  const count = el("div", "note", selected.size + " selected");
  body.append(count);

  const bar = el("div", "actionbar");
  const clear = el("button", "btn tonal", "Clear");
  clear.type = "button";
  clear.addEventListener("click", () => {
    selected.clear();
    count.textContent = t("0 selected");
    paint("");
  });
  const done = el("button", "btn", "Done");
  done.type = "button";
  done.addEventListener("click", () => param.onDone([...selected]));
  bar.append(clear, done);
  body.append(bar);

  paint("");
}

/* ---- settings, raw, backup, about, help ---- */

function viewSettings(body) {
  const theme = applyTheme();
  const box = el("div", "card pad");
  box.append(el("div", "section-title", "Theme"));
  const seg = el("div", "segmented");
  for (const t of [
    { id: "system", label: "System" },
    { id: "light", label: "Light" },
    { id: "dark", label: "Dark" },
  ]) {
    const b = el("button", null, t.label);
    b.type = "button";
    b.setAttribute("aria-pressed", String(theme === t.id));
    b.addEventListener("click", () => setTheme(t.id));
    seg.append(b);
  }
  box.append(seg);
  body.append(box);

  const def = el("div", "card pad");
  def.append(el("div", "section-title", "Default for apps without a mode of their own"));
  const modeSeg = el("div", "segmented");
  for (const m of [
    { id: "blacklist", label: "Blacklist" },
    { id: "whitelist", label: "Whitelist" },
  ]) {
    const b = el("button", null, m.label);
    b.type = "button";
    b.setAttribute("aria-pressed", String(state.config.mode === m.id));
    b.addEventListener("click", () =>
      change("Default mode", (c) => {
        c.mode = m.id;
      }),
    );
    modeSeg.append(b);
  }
  def.append(modeSeg);
  def.append(
    note(
      "Blacklist hides the apps a rule lists. Whitelist hides every app except the ones a rule lists -- an app with a rule and nothing listed is hidden from everything, so check the rules before switching.",
    ),
  );
  def.append(
    row({
      lead: leadIcon("filter"),
      title: "Hide system apps too",
      sub: "off by default: a framework package made absent breaks a device",
      trail: [
        switch_(!!state.config.hide_system, (on) =>
          change("Hide system apps", (c) => {
            c.hide_system = on;
          }),
        ),
      ],
    }),
  );
  body.append(def);

  body.append(
    card([
      row({
        lead: leadIcon("code"),
        title: "Raw config",
        sub: "edit the JSON by hand",
        trail: [icon("next")],
        onClick: () => go("raw"),
      }),
      row({
        lead: leadIcon("save"),
        title: "Backup & restore",
        sub: "copy or paste a config",
        trail: [icon("next")],
        onClick: () => go("backup"),
      }),
      row({
        lead: leadIcon("info"),
        title: "About",
        trail: [icon("next")],
        onClick: () => go("about"),
      }),
    ]),
  );

  body.append(
    note(
      t("Config format version ") +
        (state.config.version || 2) +
        t(". Every change is written through sync-tool, which keeps one .bak and pushes the new policy to the kernel in the same step."),
    ),
  );
}

function viewRaw(body) {
  const area = el("textarea", "field");
  area.spellcheck = false;
  area.value = JSON.stringify(state.config, null, 2);
  body.append(area);

  const bar = el("div", "actionbar");
  const revert = el("button", "btn tonal", "Revert");
  revert.type = "button";
  revert.addEventListener("click", () => {
    area.value = JSON.stringify(state.config, null, 2);
  });
  const save = el("button", "btn", "Validate & save");
  save.type = "button";
  save.addEventListener("click", async () => {
    let parsed;
    try {
      parsed = JSON.parse(area.value);
    } catch (e) {
      toast(t("Not valid JSON: ") + e.message, "bad");
      return;
    }
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
      toast("The config has to be a JSON object", "bad");
      return;
    }
    await change("Save", (c) => {
      for (const k of Object.keys(c)) delete c[k];
      Object.assign(c, normalize(parsed));
    });
  });
  bar.append(revert, save);
  body.append(bar);
}

function viewBackup(body) {
  const area = el("textarea", "field");
  area.spellcheck = false;
  area.value = JSON.stringify(state.config, null, 2);
  area.readOnly = true;
  area.style.minHeight = "160px";
  body.append(el("div", "section-title", "Current config"));
  body.append(area);

  const copyRow = el("div", "btnrow");
  const copy = el("button", "btn tonal", "Copy");
  copy.type = "button";
  copy.addEventListener("click", () => {
    area.select();
    let ok = false;
    try {
      ok = document.execCommand("copy");
    } catch (e) {
      ok = false;
    }
    if (!ok && navigator.clipboard) {
      navigator.clipboard.writeText(area.value).then(
        () => toast("Copied"),
        () => toast("Copy failed", "bad"),
      );
      return;
    }
    toast(ok ? "Copied" : "Copy failed", ok ? null : "bad");
  });
  const path = el("button", "btn tonal", "Copy path");
  path.type = "button";
  path.addEventListener("click", () => {
    const p = (state.status && state.status.config) || "/data/adb/hma-uidfake/config.json";
    if (navigator.clipboard) navigator.clipboard.writeText(p);
    toast(p);
  });
  copyRow.append(copy, path);
  body.append(copyRow);

  body.append(el("div", "section-title", "Restore"));
  const paste = el("textarea", "field");
  paste.spellcheck = false;
  paste.placeholder = t("paste a config here");
  paste.style.minHeight = "160px";
  body.append(paste);

  const bar = el("div", "actionbar");
  const restore = el("button", "btn", "Validate & write");
  restore.type = "button";
  restore.addEventListener("click", async () => {
    let parsed;
    try {
      parsed = JSON.parse(paste.value);
    } catch (e) {
      toast(t("Not valid JSON: ") + e.message, "bad");
      return;
    }
    await change("Restore", (c) => {
      for (const k of Object.keys(c)) delete c[k];
      Object.assign(c, normalize(parsed));
    });
  });
  bar.append(restore);
  body.append(bar);
}

function viewAbout(body) {
  const box = el("div", "card pad");
  const hs = el("div", "row");
  hs.style.padding = "0";
  const lead = el("div", "lead");
  lead.append(icon("shield"));
  hs.append(lead, el("div", "main"));
  const hm = hs.querySelector(".main");
  hm.append(el("div", "title", "HMA UID Fake fork"));
  hm.append(el("div", "sub", state.version ? "version " + state.version : "version unknown"));
  box.append(hs);
  box.append(
    note(
      languageText(
        "HMA UID Fake fork дополняет HMA/HMA-OSS: по правилам скрытия ядро отвечает на отдельные проверки UID так, будто скрываемого приложения нет. Это не заменяет фильтрацию PackageManager.",
        "A KernelSU kernel module that complements HMA/HMA-OSS by making selected UID checks behave as if a hidden app is absent. It does not replace PackageManager filtering."
      ),
    ),
  );
  body.append(box);

  body.append(
    card([
      row({
        lead: leadIcon("info"),
        title: "Source",
        sub: REPO_URL.replace("https://", ""),
        subMono: true,
        onClick: () => window.open(REPO_URL, "_blank"),
      }),
      row({
        lead: leadIcon("info"),
        title: "Licence",
        sub: "GPL-2.0",
        subMono: true,
      }),
      row({
        lead: leadIcon("help"),
        title: "How it works",
        trail: [icon("next")],
        onClick: () => go("help"),
      }),
    ]),
  );

  body.append(
    note(
      languageText(
        "Интерфейс повторяет знакомую структуру меню HMA-OSS, но реализован отдельно для ядрового модуля. Исходный код HMA-OSS здесь не копируется.",
        "The layout follows familiar HMA-OSS menus, but this WebUI was reimplemented independently for this kernel module without copying HMA-OSS source code."
      ),
    ),
  );
}

function viewHelp(body) {
  const box = el("div", "card pad");
  box.append(el("div", "section-title", languageText("Что делает модуль", "What it does")));
  box.append(note(languageText(
    "Для заданного проверяющего приложения ядро меняет ответы отдельных вызовов, связанных с UID: указанные скрытые приложения выглядят отсутствующими. Работают правила, поступившие из HMA/HMA-OSS, либо отдельная JSON-политика WebUI.",
    "For a configured caller, the kernel adjusts selected UID queries so that specified hidden apps appear absent. Rules come from HMA/HMA-OSS or an independent WebUI JSON policy."
  )));
  box.append(el("div", "section-title", languageText("Чего модуль не делает", "What it does not do")));
  box.append(note(languageText(
    "Модуль не перехватывает Java API: PackageManager, ActivityManager и ContentProvider могут по-прежнему обнаруживать приложения. Он не скрывает иконки и не заменяет HMA-OSS. Разные прошивки могут ограничивать установку хуков.",
    "The module does not intercept Java APIs: PackageManager, ActivityManager or ContentProvider can still reveal apps. It does not hide launcher icons or replace HMA-OSS. Vendor kernels may also limit which hooks can be installed."
  )));
  box.append(el("div", "section-title", languageText("Где хранятся настройки", "Where the config lives")));
  const configPath = (state.status && state.status.config) || "/data/adb/hma-uidfake/config.json";
  box.append(note(configPath + languageText(
    " — самостоятельная конфигурация хранится вне каталога модуля. Обновление ZIP её не перезаписывает; рядом сохраняется резервная копия .bak.",
    " — standalone settings are outside the module directory, so a ZIP update does not overwrite them. One .bak backup is kept alongside."
  )));
  box.append(el("div", "section-title", languageText("Как действуют правила HMA-OSS", "How HMA-OSS rules are used")));
  box.append(note(languageText(
    "Выберите источник на главном экране: HMA/HMA-OSS синхронизирует внешние правила, Ручной использует Manage apps. Сохранение или удаление ручных правил не переключает режим. При отсутствии выбранного конфига старая UID-политика очищается.",
    "Choose the source on the home screen: HMA/HMA-OSS follows external rules, Manual uses Manage apps. Saving or deleting manual rules never switches modes. A missing selected config clears the previous UID policy."
  )));
  body.append(box);
}

/* ---- sheet ---- */

function sheet(opts) {
  const root = $("dialog");
  root.replaceChildren();
  root.classList.remove("hidden");
  root.addEventListener("click", (ev) => {
    if (ev.target === root) closeSheet();
  });

  const box = el("div", "sheet");
  box.append(el("div", "grabber"));
  box.append(el("h3", null, opts.title));
  if (opts.text) box.append(el("p", null, opts.text));

  const wrap = el("div");
  const read = opts.body ? opts.body(wrap) : null;
  box.append(wrap);

  const bar = el("div", "btnrow");
  const cancel = el("button", "btn tonal", "Cancel");
  cancel.type = "button";
  cancel.addEventListener("click", closeSheet);
  const ok = el("button", "btn", opts.confirm || "OK");
  ok.type = "button";
  ok.addEventListener("click", async () => {
    const keep = await opts.onConfirm(read);
    if (!keep) closeSheet();
  });
  if (!opts.infoOnly) bar.append(cancel);
  bar.append(ok);
  box.append(bar);

  root.append(box);
}

function closeSheet() {
  $("dialog").classList.add("hidden");
  $("dialog").replaceChildren();
}

/* ---- boot ---- */

applyTheme();
render();
load();
