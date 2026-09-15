// Fetches GitHub releases for lmriccardo/a-simple-game-engine and renders them.
(function () {
  "use strict";

  var REPO = "lmriccardo/a-simple-game-engine";
  var API_URL = "https://api.github.com/repos/" + REPO + "/releases?per_page=20";
  var listEl = document.getElementById("releases-list");
  if (!listEl) return;

  function formatDate(iso) {
    try {
      return new Date(iso).toLocaleDateString(undefined, { year: "numeric", month: "long", day: "numeric" });
    } catch (e) {
      return iso;
    }
  }

  function renderBody(markdown) {
    if (window.marked && window.DOMPurify) {
      var html = window.marked.parse(markdown || "");
      return window.DOMPurify.sanitize(html);
    }
    var div = document.createElement("div");
    div.textContent = markdown || "";
    return "<pre>" + div.innerHTML + "</pre>";
  }

  function humanSize(bytes) {
    if (!bytes) return "";
    var units = ["B", "KB", "MB", "GB"];
    var i = 0;
    while (bytes >= 1024 && i < units.length - 1) {
      bytes /= 1024;
      i++;
    }
    return bytes.toFixed(bytes >= 10 || i === 0 ? 0 : 1) + " " + units[i];
  }

  function releaseCard(rel) {
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

    var badges = document.createElement("div");
    badges.style.marginTop = "6px";
    badges.style.display = "flex";
    badges.style.gap = "8px";
    if (rel.prerelease) {
      var pre = document.createElement("span");
      pre.className = "tag";
      pre.textContent = "pre-release";
      badges.appendChild(pre);
    }
    if (rel.draft) {
      var draft = document.createElement("span");
      draft.className = "tag";
      draft.textContent = "draft";
      badges.appendChild(draft);
    }
    var tagEl = document.createElement("span");
    tagEl.className = "tag mono";
    tagEl.textContent = rel.tag_name;
    badges.appendChild(tagEl);
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

    if (rel.assets && rel.assets.length) {
      var assetsWrap = document.createElement("div");
      assetsWrap.className = "assets";
      rel.assets.forEach(function (asset) {
        var a = document.createElement("a");
        a.href = asset.browser_download_url;
        a.className = "btn btn-ghost";
        a.style.padding = "6px 12px";
        a.style.fontSize = "0.82rem";
        a.textContent = asset.name + (asset.size ? " (" + humanSize(asset.size) + ")" : "");
        assetsWrap.appendChild(a);
      });
      card.appendChild(assetsWrap);
    }

    var ghLink = document.createElement("div");
    ghLink.style.marginTop = "12px";
    var a2 = document.createElement("a");
    a2.href = rel.html_url;
    a2.target = "_blank";
    a2.rel = "noopener";
    a2.textContent = "View on GitHub →";
    a2.style.fontSize = "0.85rem";
    a2.style.fontWeight = "600";
    ghLink.appendChild(a2);
    card.appendChild(ghLink);

    return card;
  }

  function skeletons(n) {
    var frag = document.createDocumentFragment();
    for (var i = 0; i < n; i++) {
      var card = document.createElement("div");
      card.className = "release-card";
      card.innerHTML =
        '<div class="skeleton" style="width:220px;height:22px;margin-bottom:14px;"></div>' +
        '<div class="skeleton" style="width:100%;height:14px;margin-bottom:8px;"></div>' +
        '<div class="skeleton" style="width:80%;height:14px;"></div>';
      frag.appendChild(card);
    }
    return frag;
  }

  listEl.appendChild(skeletons(3));

  fetch(API_URL, { headers: { Accept: "application/vnd.github+json" } })
    .then(function (res) {
      if (!res.ok) throw new Error("GitHub API responded with " + res.status);
      return res.json();
    })
    .then(function (releases) {
      listEl.innerHTML = "";
      if (!releases.length) {
        listEl.innerHTML = '<p class="lede">No releases published yet — check the <a href="https://github.com/' +
          REPO + '/tags" target="_blank" rel="noopener">tags page</a> on GitHub.</p>';
        return;
      }
      releases.forEach(function (rel) {
        listEl.appendChild(releaseCard(rel));
      });
    })
    .catch(function (err) {
      listEl.innerHTML =
        '<p class="error-note">Couldn\'t load releases from the GitHub API (' + err.message +
        '). You can browse them directly on <a href="https://github.com/' + REPO +
        '/releases" target="_blank" rel="noopener">GitHub</a> instead.</p>';
    });
})();
