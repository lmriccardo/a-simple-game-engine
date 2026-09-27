// Fetches GitHub releases and renders a per-version table of asge-editor
// native installer downloads (see release-on-tag.yml / CMakeLists.txt CPack
// section for how these assets get built and named).
(function () {
  "use strict";

  var REPO = "lmriccardo/a-simple-game-engine";
  var API_URL = "https://api.github.com/repos/" + REPO + "/releases?per_page=30";
  var container = document.getElementById("editor-releases-table");
  if (!container) return;

  var PLATFORMS = [
    { label: "Windows", match: /asge-editor-.*-windows-x64\.exe$/i },
    { label: "Linux", match: /asge-editor-.*-linux-x64\.deb$/i },
    { label: "macOS", match: /asge-editor-.*-macos-arm64\.dmg$/i }
  ];

  function findAsset(assets, re) {
    for (var i = 0; i < assets.length; i++) {
      if (re.test(assets[i].name)) return assets[i];
    }
    return null;
  }

  function cell(asset) {
    if (!asset) return '<span style="color:var(--text-muted);">&mdash;</span>';
    var div = document.createElement("div");
    div.textContent = asset.name;
    return '<a class="btn btn-ghost" style="padding:6px 12px;font-size:0.82rem;white-space:nowrap;" href="' +
      asset.browser_download_url + '">' + div.innerHTML + "</a>";
  }

  function buildTable(releases) {
    var rows = releases.map(function (rel) {
      var assets = rel.assets || [];
      var tds = PLATFORMS.map(function (p) {
        return "<td>" + cell(findAsset(assets, p.match)) + "</td>";
      }).join("");
      var tagDiv = document.createElement("div");
      tagDiv.textContent = rel.tag_name;
      return '<tr><td><a href="' + rel.html_url + '" target="_blank" rel="noopener"><strong>' +
        tagDiv.innerHTML + "</strong></a></td>" + tds + "</tr>";
    }).join("");
    return "<table><thead><tr><th>Version</th><th>Windows</th><th>Linux</th><th>macOS</th></tr></thead><tbody>" +
      rows + "</tbody></table>";
  }

  container.innerHTML = '<p class="lede">Loading releases&hellip;</p>';

  fetch(API_URL, { headers: { Accept: "application/vnd.github+json" } })
    .then(function (res) {
      if (!res.ok) throw new Error("GitHub API responded with " + res.status);
      return res.json();
    })
    .then(function (releases) {
      var published = releases.filter(function (rel) { return !rel.draft; });
      if (!published.length) {
        container.innerHTML = '<p class="lede">No releases published yet &mdash; check the <a href="https://github.com/' +
          REPO + '/tags" target="_blank" rel="noopener">tags page</a> on GitHub.</p>';
        return;
      }
      container.innerHTML = buildTable(published);
    })
    .catch(function (err) {
      container.innerHTML =
        '<p class="error-note">Couldn\'t load releases from the GitHub API (' + err.message +
        '). Browse them directly on <a href="https://github.com/' + REPO +
        '/releases" target="_blank" rel="noopener">GitHub</a> instead.</p>';
    });
})();
