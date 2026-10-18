/* Verify context-help coverage and English/Russian explanations without Android. */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");

const helpSource = fs.readFileSync("module/webroot/help.js", "utf8");
const uiSource = fs.readFileSync("module/webroot/app.js", "utf8");
const html = fs.readFileSync("module/webroot/index.html", "utf8");
const css = fs.readFileSync("module/webroot/style.css", "utf8");
const sandbox = { window: {} };
vm.createContext(sandbox);
vm.runInContext(helpSource, sandbox);
const topics = sandbox.window.HMA_CONTEXT_HELP;

const screens = [
  "home", "apps", "app", "templates", "template", "picker",
  "pickerTemplates", "settings", "raw", "backup", "about", "help"
];
for (const key of screens) {
  assert(topics[key], "Missing help topic " + key);
  for (const locale of ["ru", "en"]) {
    const text = topics[key][locale];
    assert(text && typeof text.title === "string" && text.title.length > 4);
    assert(typeof text.intro === "string" && text.intro.length > 40);
    assert(text.sections.length >= 2, key + " / " + locale);
    for (const section of text.sections) {
      assert(section.title.length > 2 && section.text.length > 35);
    }
  }
}
assert(topics.apps.ru.intro.includes("самостоятельные"));
assert(topics.home.ru.sections.some(s => s.text.includes("no rules yet")));
assert(topics.app.ru.sections.some(s => s.text.includes("HMA-OSS")));
assert(topics.settings.en.sections.some(s => s.text.includes("priority")));
assert(html.indexOf('src="help.js"') < html.indexOf('src="app.js"'));
assert(uiSource.includes("function showContextHelp(key)"));
assert(uiSource.includes("function screenHelpKey(s)"));
assert(uiSource.includes("actions.push("));
assert(uiSource.includes("infoOnly: true"));
assert(uiSource.includes('languageText("Что делает модуль", "What it does")'));
assert(uiSource.includes('languageText("Где хранятся настройки", "Where the config lives")'));
for (const screen of ["apps", "templates", "backup", "help", "settings", "about"]) {
  assert(uiSource.includes('helpButton("' + screen + '")'), screen);
}
assert(css.includes(".help-inline"));
assert(css.includes(".context-help-paragraph"));
assert(!uiSource.includes('const REPO_URL = "https://github.com/SHuiTU68'));
console.log("OK: 12 contextual-help screens, RU/EN coverage, English menu labels and About.");
