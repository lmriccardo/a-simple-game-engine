// Simple client-side substring search over api/search-index.json for the
// API reference sidebar. Every api/*.html page sits at the same depth, so
// a plain relative "search-index.json" resolves from any of them.
(function () {
  "use strict";

  var input = document.getElementById("api-search-input");
  var resultsBox = document.getElementById("api-search-results");
  var tree = document.getElementById("api-tree");
  if (!input || !resultsBox || !tree) return;

  var index = null;
  fetch("search-index.json")
    .then(function (res) { return res.json(); })
    .then(function (data) { index = data; })
    .catch(function () { /* search just won't work; the tree still does */ });

  function render(matches, query) {
    resultsBox.innerHTML = "";
    if (!matches.length) {
      resultsBox.innerHTML = '<div class="api-search-empty">No matches for "' + escapeHtml(query) + '"</div>';
      return;
    }
    matches.slice(0, 30).forEach(function (m) {
      var a = document.createElement("a");
      a.href = m.url;
      var name = document.createElement("span");
      name.textContent = m.name;
      var kind = document.createElement("span");
      kind.className = "tag";
      kind.textContent = m.kind;
      a.appendChild(name);
      a.appendChild(kind);
      resultsBox.appendChild(a);
    });
  }

  function escapeHtml(s) {
    var div = document.createElement("div");
    div.textContent = s;
    return div.innerHTML;
  }

  input.addEventListener("input", function () {
    var query = input.value.trim().toLowerCase();
    if (!query || !index) {
      resultsBox.hidden = true;
      tree.hidden = false;
      return;
    }
    tree.hidden = true;
    resultsBox.hidden = false;
    var matches = index.filter(function (m) { return m.name.toLowerCase().indexOf(query) !== -1; });
    render(matches, query);
  });
})();
