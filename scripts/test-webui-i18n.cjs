/* Verify locale selection and the visible labels independently from KernelSU. */
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const source = fs.readFileSync("module/webroot/i18n.js", "utf8");
for (const [stored, browser, expected, label] of [
  ["ru-RU", "en-US", "ru", "Управление приложениями"],
  ["ru", "en-US", "ru", "Управление приложениями"],
  ["en-US", "ru-RU", "en", "Manage apps"],
  ["de-DE", "ru-RU", "en", "Manage apps"],
  ["", "ru-RU", "ru", "Управление приложениями"],
  ["", "en-US", "en", "Manage apps"]
]) {
  const sandbox = {
    window: { HMA_UIDFAKE_INSTALL_LOCALE: stored },
    navigator: { language: browser, languages: [browser] },
    document: { documentElement: { lang: "en" } }
  };
  vm.createContext(sandbox);
  vm.runInContext(source, sandbox);
  assert.equal(vm.runInContext("HMA_UI_LOCALE", sandbox), expected);
  assert.equal(sandbox.document.documentElement.lang, expected);
  assert.equal(vm.runInContext('t("Manage apps")', sandbox), label);
  assert.equal(vm.runInContext('t("com.example.keep-original")', sandbox), "com.example.keep-original");
  assert.equal(vm.runInContext('t("uid 12345")', sandbox), "uid 12345");
}
console.log("OK: Russian/English system locale and text smoke tests");
