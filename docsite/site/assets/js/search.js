// ASGE docsite — global search (Ctrl+K). Loads assets/search-index.json and, when it
// exists, engine/api/search-index.json (built in CI), then ranks matches client-side.
// Queries are matched as literal substrings; no regular expression is built from input.
(function () {
  "use strict";

  var MAX_RESULTS = 30;

  function rank(index, query) {
    var tokens = String(query || "").toLowerCase().split(/\s+/).filter(Boolean);
    if (!tokens.length) return [];
    var scored = [];
    index.forEach(function (entry) {
      var title = (entry.title || "").toLowerCase();
      var rest = ((entry.section || "") + " " + (entry.text || "")).toLowerCase();
      var score = 0;
      for (var i = 0; i < tokens.length; i++) {
        var t = tokens[i];
        var inTitle = title.indexOf(t);
        if (inTitle !== -1) score += inTitle === 0 ? 5 : 3;
        else if (rest.indexOf(t) !== -1) score += 1;
        else return;
      }
      scored.push({ entry: entry, score: score });
    });
    scored.sort(function (a, b) { return b.score - a.score || a.entry.title.localeCompare(b.entry.title); });
    return scored.map(function (s) { return s.entry; });
  }

  if (typeof module !== "undefined" && module.exports) module.exports = { rank: rank };
  if (typeof document === "undefined") return;

  var modal, input, results, root, index = null, active = -1;

  function load() {
    if (index) return Promise.resolve(index);
    function get(url) {
      return fetch(root + url).then(function (r) { return r.ok ? r.json() : []; }).catch(function () { return []; });
    }
    return Promise.all([get("assets/search-index.json"), get("engine/api/search-index.json")]).then(function (parts) {
      index = parts[0].concat(parts[1]);
      return index;
    });
  }

  function items() { return results.querySelectorAll("a"); }

  function select(n) {
    var links = items();
    if (!links.length) { active = -1; return; }
    active = (n + links.length) % links.length;
    links.forEach(function (a, i) { a.setAttribute("aria-selected", i === active ? "true" : "false"); });
    links[active].scrollIntoView({ block: "nearest" });
  }

  function render(query) {
    results.textContent = "";
    active = -1;
    if (!query.trim()) return;
    var matches = rank(index || [], query).slice(0, MAX_RESULTS);
    if (!matches.length) {
      var empty = document.createElement("div");
      empty.className = "search-empty";
      empty.textContent = "No results for “" + query.trim() + "”";
      results.appendChild(empty);
      return;
    }
    var last = null;
    matches.forEach(function (m) {
      if (m.section !== last) {
        var h = document.createElement("h5");
        h.textContent = m.section;
        results.appendChild(h);
        last = m.section;
      }
      var a = document.createElement("a");
      a.href = root + m.url;
      a.textContent = m.title;
      results.appendChild(a);
    });
    select(0);
  }

  function open() {
    if (!modal.open) modal.showModal();
    input.value = "";
    results.textContent = "";
    input.focus();
    load();
  }

  document.addEventListener("DOMContentLoaded", function () {
    modal = document.querySelector(".search-modal");
    if (!modal) return;
    input = modal.querySelector("input");
    results = modal.querySelector(".search-results");
    root = document.body.getAttribute("data-root") || "./";

    document.querySelectorAll("[data-search-open]").forEach(function (b) { b.addEventListener("click", open); });
    document.addEventListener("keydown", function (e) {
      var typing = /^(INPUT|TEXTAREA|SELECT)$/.test((e.target && e.target.tagName) || "");
      if ((e.key === "k" && (e.ctrlKey || e.metaKey)) || (e.key === "/" && !typing)) {
        e.preventDefault();
        open();
      }
    });
    modal.addEventListener("click", function (e) { if (e.target === modal) modal.close(); });
    input.addEventListener("input", function () { load().then(function () { render(input.value); }); });
    input.addEventListener("keydown", function (e) {
      if (e.key === "ArrowDown") { e.preventDefault(); select(active + 1); }
      else if (e.key === "ArrowUp") { e.preventDefault(); select(active - 1); }
      else if (e.key === "Enter" && active >= 0) { e.preventDefault(); items()[active].click(); }
    });
  });
})();
