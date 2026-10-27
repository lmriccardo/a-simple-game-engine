// Run: node docsite/tests/search_test.js
const assert = require("assert");
const { rank } = require("../site/assets/js/search.js");

const index = [
  { title: "ECS", url: "a.html", section: "Theory", text: "sparse set pools registry" },
  { title: "Registry", url: "b.html", section: "API", text: "Class" },
  { title: "Getting Started", url: "c.html", section: "Start", text: "cmake build ecs" },
];

assert.deepStrictEqual(rank(index, "ecs").map((r) => r.url), ["a.html", "c.html"], "title match outranks text match");
assert.deepStrictEqual(rank(index, "sparse registry").map((r) => r.url), ["a.html"], "every token must match");
assert.deepStrictEqual(rank(index, "   "), [], "blank query matches nothing");
for (const q of ["(", "[a-", "\\", "a.*", "?"]) assert.doesNotThrow(() => rank(index, q), q);
assert.deepStrictEqual(rank(index, "[a-"), [], "regex characters are literal");
console.log("search_test ok");
