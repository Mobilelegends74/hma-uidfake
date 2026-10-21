"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const source = fs.readFileSync("module/webroot/app.js", "utf8");
const constants = source.slice(source.indexOf("const MANUAL_KMIS"), source.indexOf("/* ---- talking"));
const ui = source.slice(source.indexOf("let manualInjectRunning"), source.indexOf("function confirmManualInject"));
function el(tag, cls, text) {
  return { tag, cls, text, children: [], append(...items) { this.children.push(...items); },
    addEventListener(event, fn) { this[event] = fn; } };
}
(async () => {
  let sheet;
  let release = "Linux version 5.15.220-Wild (builder)";
  let clicks = [];
  const sandbox = { el, note: (text) => el("p", null, text), firstLine: (text) => text,
    api: { kernelVersion: async () => release }, languageText: (ru, en) => en,
    sheet: (opts) => { sheet = opts; }, confirmManualInject: (candidate) => clicks.push(candidate) };
  vm.createContext(sandbox);
  vm.runInContext(constants + ui, sandbox);
  for (const [branch, count] of [["5.10", 2], ["5.15", 2], ["6.1", 1], ["6.6", 1], ["6.12", 2], ["6.18", 1]]) {
    release = `Linux version ${branch}.123-custom (builder)`;
    await sandbox.showManualInject();
    const wrap = el("div");
    sheet.body(wrap);
    const grid = wrap.children[0];
    assert.equal(grid.cls, "manual-kmi-grid");
    assert.equal(grid.children.length, count);
    for (const button of grid.children) {
      assert.equal(button.type, "button");
      assert.equal(button.cls, "btn tonal manual-kmi-choice");
      assert(!button.children[0].text.toLowerCase().includes("oneplus"));
      button.click();
      assert.equal(clicks.at(-1).split("-")[1], branch);
      if (clicks.at(-1).includes("oneplus")) assert.equal(button.children[0].text, "android16-6.12.38");
    }
  }
  for (const unknown of ["", "Linux version 4.19.0-custom", "Android 16", "Linux version 5.150.1"]) {
    release = unknown;
    await sandbox.showManualInject();
    const wrap = el("div"); sheet.body(wrap);
    assert.equal(wrap.children[0].tag, "p", "unknown/unsupported branches must not expose all modules");
  }
  sandbox.api.kernelVersion = async () => { throw new Error("read denied"); };
  await sandbox.showManualInject();
  const wrap = el("div"); sheet.body(wrap);
  assert.equal(wrap.children[0].text, "read denied");
  assert.equal(vm.runInContext("manualChoicesLoading", sandbox), false);
  const css = fs.readFileSync("module/webroot/style.css", "utf8");
  assert.match(css, /grid-template-columns: repeat\(2, minmax\(0, 1fr\)\)/);
  console.log("Manual inject: kernel-family filtering, grid, selection and read failures passed");
})().catch((err) => { console.error(err); process.exitCode = 1; });
