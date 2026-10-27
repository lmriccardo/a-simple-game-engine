// Run: node docsite/tests/releases_test.js
const assert = require("assert");
const { classify, visible, route } = require("../site/assets/js/releases.js");

assert.deepStrictEqual(classify({ tag_name: "v0.8.8e1.5", assets: [] }), { engine: "0.8.8", editor: "1.5" });
assert.deepStrictEqual(classify({ tag_name: "v0.8.4", assets: [] }), { engine: "0.8.4", editor: null });
assert.strictEqual(classify({ tag_name: "v0.9.0", assets: [{ name: "asge-editor-0.9.0-linux-x64.deb" }] }).editor, "installer");

const all = [
  { tag_name: "v0.8.8e1.5", assets: [] },
  { tag_name: "v0.8.4", assets: [] },
  { tag_name: "v0.8.0", draft: true, assets: [] },
];
assert.deepStrictEqual(visible(all, "all").map((r) => r.tag_name), ["v0.8.8e1.5", "v0.8.4"], "drafts are hidden");
assert.deepStrictEqual(visible(all, "editor").map((r) => r.tag_name), ["v0.8.8e1.5"]);
assert.deepStrictEqual(visible(all, "engine").map((r) => r.tag_name), ["v0.8.8e1.5", "v0.8.4"]);
assert.deepStrictEqual(visible([], "all"), []);

assert.deepStrictEqual(route("", all), { view: "list" });
assert.deepStrictEqual(route("#", all), { view: "list" });
assert.strictEqual(route("#v0.8.4", all).view, "detail");
assert.strictEqual(route("#v0.8.4", all).release.tag_name, "v0.8.4");
assert.deepStrictEqual(route("#v9.9.9", all), { view: "missing", tag: "v9.9.9" }, "unknown tag");
assert.deepStrictEqual(route("#v0.8.0", all), { view: "missing", tag: "v0.8.0" }, "drafts have no detail view");
assert.deepStrictEqual(route("#%3Cscript%3E", all), { view: "missing", tag: "<script>" }, "tag is decoded, never trusted");
console.log("releases_test ok");
