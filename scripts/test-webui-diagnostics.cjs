/* Diagnostic UI smoke tests without Android or a browser dependency. */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const source = fs.readFileSync("module/webroot/app.js", "utf8");
const ui = source.slice(source.indexOf("function viewHome(body)"), source.indexOf("function leadIcon(name)"));
function node() { return { children: [], append(...items) { this.children.push(...items); }, style: {}, addEventListener() {}, setAttribute() {} }; }

(async () => {
  for (const russian of [false, true]) {
    let currentSheet;
    let calls = 0;
    const messages = [];
    const sandbox = {
      state: { config: { apps: {}, templates: {} }, version: "0.3.8-test" },
      languageText: (ru, en) => russian ? ru : en,
      el: node, row: (opts) => opts, card: (rows) => rows,
      statusCard: () => "status", leadIcon: () => "icon", icon: () => "icon",
      helpButton: () => "help", go() {}, render() {},
      note: (text) => text,
      sheet: (opts) => { currentSheet = opts; },
      closeSheet: () => { currentSheet = null; },
      toast: (message) => messages.push(message),
      navigator: {}, document: {},
      window: { confirm: () => true },
      api: { diagnostics: async () => { calls++; return { ok: true, path: "/storage/emulated/0/Download/uidfake-diagnostics-20261020-010101-Abc123.tar.gz" }; } },
      guard: async (_, fn) => { try { await fn(); } catch (err) { messages.push(err.message); } },
    };
    vm.createContext(sandbox);
    vm.runInContext(ui, sandbox);
    const body = node();
    sandbox.viewHome(body);
    const rows = body.children.at(-1);
    assert.equal(rows.at(-2).title, "About");
    assert.equal(rows.at(-1).title, russian ? "Сохранить диагностику" : "Save diagnostics");
    rows.at(-1).onClick();
    assert(currentSheet.text.includes(russian ? "названия приложений" : "app names"));
    assert.equal(calls, 0, "consent must precede collection");
    const firstConfirm = currentSheet.onConfirm;
    const pending = firstConfirm();
    const duplicate = firstConfirm();
    sandbox.saveDiagnostics();
    await pending;
    await duplicate;
    assert.equal(calls, 1, "duplicate action must not collect twice");
    assert.equal(currentSheet.title, russian ? "Диагностика сохранена" : "Diagnostics saved");
    assert.equal(currentSheet.infoOnly, true);
    const detail = node();
    currentSheet.body(detail);
    assert.equal(detail.children[0].readOnly, true);
    assert(detail.children[0].value.endsWith(".tar.gz"));
    assert.equal(vm.runInContext("diagnosticsRunning", sandbox), false);
    const savedConfig = JSON.stringify(sandbox.state.config);
    let sourceCalls = 0;
    sandbox.api.setSource = async (mode) => { sourceCalls++; return { rule_mode: mode, active_config: "config.json" }; };
    await sandbox.switchRuleSource("manual");
    assert.equal(sandbox.state.status.rule_mode, "manual");
    await sandbox.switchRuleSource("external");
    assert.equal(sandbox.state.status.rule_mode, "external");
    assert.equal(sourceCalls, 2);
    sandbox.window.confirm = () => false;
    await sandbox.switchRuleSource("manual");
    assert.equal(sourceCalls, 2);
    assert.equal(sandbox.state.status.rule_mode, "external");
    sandbox.window.confirm = () => true;
    sandbox.api.setSource = async () => { throw new Error("source write denied"); };
    await sandbox.switchRuleSource("manual");
    assert.equal(sandbox.state.status.rule_mode, "external");
    assert.equal(vm.runInContext("sourceSwitching", sandbox), false);
    assert.equal(JSON.stringify(sandbox.state.config), savedConfig);

    for (const bad of [null, { ok: false }, { ok: true, path: "/etc/passwd" }]) {
      sandbox.api.diagnostics = async () => bad;
      sandbox.saveDiagnostics();
      await currentSheet.onConfirm();
      assert.equal(currentSheet, null, "failure must not claim a saved archive");
      assert.equal(vm.runInContext("diagnosticsRunning", sandbox), false);
    }
    sandbox.api.diagnostics = async () => { throw new Error("storage denied"); };
    sandbox.saveDiagnostics();
    await currentSheet.onConfirm();
    assert(messages.includes("storage denied"));
    assert.equal(vm.runInContext("diagnosticsRunning", sandbox), false);
  }
  console.log("OK: RU/EN bottom-menu placement, privacy consent, duplicate prevention, saved path and collection failures");
})().catch((err) => { console.error(err); process.exitCode = 1; });
