// Fetches GitHub releases for lmriccardo/a-simple-game-engine and renders one merged
// Engine + Editor timeline. A tag like v0.8.8e1.5 is engine 0.8.8 shipped with editor 1.5;
// a plain v0.8.4 is an engine-only release. Also fills the home page's #latest-release.
(function () {
  "use strict";

  var REPO = "lmriccardo/a-simple-game-engine";
  var API_URL = "https://api.github.com/repos/" + REPO + "/releases?per_page=30";
  var TAG_RE = /^v?(\d+\.\d+\.\d+)(?:e(\d+\.\d+))?$/;
  var EDITOR_ASSET_RE = /^asge-editor-/i;

  // -> { engine: "0.8.8"|null, editor: "1.5"|"installer"|null }
  function classify(rel) {
    var m = TAG_RE.exec(rel.tag_name || "");
    var editor = m && m[2] ? m[2] : null;
    if (!editor && (rel.assets || []).some(function (a) { return EDITOR_ASSET_RE.test(a.name); })) editor = "installer";
    return { engine: m ? m[1] : null, editor: editor };
  }

  // Drafts are never shown; every release is an engine release, only some are editor releases.
  function visible(releases, filter) {
    return releases.filter(function (r) {
      return !r.draft && (filter !== "editor" || classify(r).editor);
    });
  }

  // #<tag> opens that release's own view; anything else is the list. Unknown tags and drafts are "missing".
  function route(hash, releases) {
    var tag = "";
    try { tag = decodeURIComponent((hash || "").replace(/^#/, "")); } catch (e) { tag = ""; }
    if (!tag) return { view: "list" };
    var rel = releases.filter(function (r) { return !r.draft && r.tag_name === tag; })[0];
    return rel ? { view: "detail", release: rel } : { view: "missing", tag: tag };
  }

  if (typeof module !== "undefined" && module.exports) module.exports = { classify: classify, visible: visible, route: route };
  if (typeof document === "undefined") return;

  var listEl = document.getElementById("releases-list");
  var latestEl = document.getElementById("latest-release");
  if (!listEl && !latestEl) return;
  var fallbackEl = document.getElementById("releases-fallback");

  function formatDate(iso) {
    try {
      return new Date(iso).toLocaleDateString(undefined, { year: "numeric", month: "long", day: "numeric" });
    } catch (e) {
      return iso;
    }
  }

  function renderBody(markdown) {
    if (window.marked && window.DOMPurify) {
      return window.DOMPurify.sanitize(window.marked.parse(markdown || ""));
    }
    var div = document.createElement("div");
    div.textContent = markdown || "";
    return "<pre>" + div.innerHTML + "</pre>";
  }

  function humanSize(bytes) {
    if (!bytes) return "";
    var units = ["B", "KB", "MB", "GB"], i = 0;
    while (bytes >= 1024 && i < units.length - 1) { bytes /= 1024; i++; }
    return bytes.toFixed(bytes >= 10 || i === 0 ? 0 : 1) + " " + units[i];
  }

  function tag(text, cls) {
    var s = document.createElement("span");
    s.className = "tag" + (cls ? " " + cls : "");
    s.textContent = text;
    return s;
  }

  function assetsFor(rel, filter) {
    return (rel.assets || []).filter(function (a) {
      var isEditor = EDITOR_ASSET_RE.test(a.name);
      return filter === "all" || (filter === "editor" ? isEditor : !isEditor);
    });
  }

  function badgesFor(rel) {
    var info = classify(rel);
    var badges = document.createElement("div");
    badges.style.cssText = "display:flex;gap:8px;flex-wrap:wrap;";
    if (info.engine) badges.appendChild(tag("Engine " + info.engine));
    if (info.editor) badges.appendChild(tag(info.editor === "installer" ? "Editor" : "Editor " + info.editor));
    if (rel.prerelease) badges.appendChild(tag("pre-release"));
    return badges;
  }

  function assetButtons(rel, filter) {
    var wrap = document.createElement("div");
    wrap.className = "assets";
    assetsFor(rel, filter).forEach(function (asset) {
      var a = document.createElement("a");
      a.href = asset.browser_download_url;
      a.className = "btn btn-ghost";
      a.style.cssText = "padding:6px 12px;font-size:0.82rem;";
      a.textContent = asset.name + (asset.size ? " (" + humanSize(asset.size) + ")" : "");
      wrap.appendChild(a);
    });
    return wrap;
  }

  // One compact line of the list; the full notes live in the detail view.
  function releaseRow(rel, filter) {
    var row = document.createElement("article");
    row.className = "release-row";
    var link = document.createElement("a");
    link.className = "release-row-title";
    link.href = "#" + encodeURIComponent(rel.tag_name);
    link.textContent = rel.name || rel.tag_name;
    row.appendChild(link);
    row.appendChild(badgesFor(rel));
    var time = document.createElement("time");
    time.dateTime = rel.published_at || rel.created_at;
    time.textContent = formatDate(rel.published_at || rel.created_at);
    row.appendChild(time);
    if (filter === "editor") row.appendChild(assetButtons(rel, "editor"));
    return row;
  }

  function releaseCard(rel, filter) {
    var card = document.createElement("article");
    card.className = "release-card";

    var head = document.createElement("div");
    head.className = "release-head";
    var titleWrap = document.createElement("div");
    var h3 = document.createElement("h3");
    var link = document.createElement("a");
    link.href = rel.html_url;
    link.target = "_blank";
    link.rel = "noopener";
    link.textContent = rel.name || rel.tag_name;
    h3.appendChild(link);
    titleWrap.appendChild(h3);

    var badges = badgesFor(rel);
    badges.style.marginTop = "6px";
    badges.appendChild(tag(rel.tag_name, "mono"));
    titleWrap.appendChild(badges);
    head.appendChild(titleWrap);

    var time = document.createElement("time");
    time.dateTime = rel.published_at || rel.created_at;
    time.textContent = formatDate(rel.published_at || rel.created_at);
    head.appendChild(time);
    card.appendChild(head);

    var body = document.createElement("div");
    body.className = "release-body";
    body.innerHTML = renderBody(rel.body);
    card.appendChild(body);

    if (assetsFor(rel, filter).length) card.appendChild(assetButtons(rel, filter));
    return card;
  }

  function showError(message) {
    if (fallbackEl) {
      fallbackEl.hidden = false;
      fallbackEl.className = "error-note";
      fallbackEl.insertAdjacentText("afterbegin", message + " ");
    }
    if (listEl) listEl.textContent = "";
  }

  var releases = [];
  var filter = "all";

  function render() {
    if (!listEl) return;
    listEl.textContent = "";
    var filters = document.getElementById("releases-filters");
    var r = route(location.hash, releases);
    if (filters) filters.hidden = r.view !== "list";
    if (r.view !== "list") {
      var back = document.createElement("a");
      back.href = "#";
      back.className = "back-link";
      back.textContent = "← All releases";
      listEl.appendChild(back);
      if (r.view === "detail") listEl.appendChild(releaseCard(r.release, "all"));
      else {
        var p = document.createElement("p");
        p.className = "lede";
        p.textContent = "No release is tagged “" + r.tag + "”.";
        listEl.appendChild(p);
      }
      window.scrollTo(0, 0);
      return;
    }
    var shown = visible(releases, filter);
    if (!shown.length) {
      listEl.innerHTML = '<p class="lede">No matching releases yet - see the <a href="https://github.com/' +
        REPO + '/tags" target="_blank" rel="noopener">tags page</a> on GitHub.</p>';
      return;
    }
    shown.forEach(function (rel) { listEl.appendChild(releaseRow(rel, filter)); });
  }

  window.addEventListener("hashchange", render);

  document.querySelectorAll("[data-filter]").forEach(function (btn) {
    btn.addEventListener("click", function () {
      filter = btn.getAttribute("data-filter");
      document.querySelectorAll("[data-filter]").forEach(function (b) {
        b.setAttribute("aria-pressed", b === btn ? "true" : "false");
      });
      render();
    });
  });

  fetch(API_URL, { headers: { Accept: "application/vnd.github+json" } })
    .then(function (res) {
      if (!res.ok) throw new Error("GitHub API responded with " + res.status);
      return res.json();
    })
    .then(function (data) {
      releases = Array.isArray(data) ? data : [];
      var published = visible(releases, "all");
      if (!published.length) {
        showError("No releases are published yet.");
        return;
      }
      if (fallbackEl) fallbackEl.hidden = true;
      render();
      if (latestEl) {
        var latest = published[0], a = document.createElement("a");
        a.href = "releases.html#" + encodeURIComponent(latest.tag_name);
        a.textContent = latest.name || latest.tag_name;
        latestEl.textContent = "Latest release: ";
        latestEl.appendChild(a);
        latestEl.hidden = false;
      }
    })
    .catch(function (err) {
      showError("Couldn't load releases from the GitHub API (" + err.message + ").");
    });
})();
