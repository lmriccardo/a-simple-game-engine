// ASGE docsite — shared behaviour: theme toggle, mobile nav, code copy buttons, tabs.
(function () {
  "use strict";

  var THEME_KEY = "asge-theme";

  function applyTheme(theme) {
    if (theme === "light" || theme === "dark") {
      document.documentElement.setAttribute("data-theme", theme);
    } else {
      document.documentElement.removeAttribute("data-theme");
    }
  }

  function currentTheme() {
    try {
      return localStorage.getItem(THEME_KEY);
    } catch (e) {
      return null;
    }
  }

  function systemPrefersDark() {
    return window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches;
  }

  applyTheme(currentTheme());

  function initThemeToggle() {
    var btn = document.querySelector("[data-theme-toggle]");
    if (!btn) return;
    btn.addEventListener("click", function () {
      var stored = currentTheme();
      var effectiveDark = stored ? stored === "dark" : systemPrefersDark();
      var next = effectiveDark ? "light" : "dark";
      applyTheme(next);
      try {
        localStorage.setItem(THEME_KEY, next);
      } catch (e) {
        /* private browsing / storage disabled — theme just won't persist */
      }
    });
  }

  function initNavToggle() {
    var toggle = document.querySelector("[data-nav-toggle]");
    if (!toggle) return;
    toggle.addEventListener("click", function () {
      var open = document.body.classList.toggle("drawer-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
  }

  function initTocSpy() {
    var links = document.querySelectorAll(".toc a[href^='#']");
    if (!links.length || !("IntersectionObserver" in window)) return;
    var byId = {};
    links.forEach(function (a) { byId[a.getAttribute("href").slice(1)] = a; });
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (!e.isIntersecting) return;
        links.forEach(function (a) { a.classList.remove("active"); });
        var a = byId[e.target.id];
        if (a) a.classList.add("active");
      });
    }, { rootMargin: "-80px 0px -70% 0px" });
    Object.keys(byId).forEach(function (id) {
      var el = document.getElementById(id);
      if (el) observer.observe(el);
    });
  }

  function initCopyButtons() {
    document.querySelectorAll("[data-copy]").forEach(function (btn) {
      btn.addEventListener("click", function () {
        var targetSel = btn.getAttribute("data-copy");
        var el = targetSel ? document.querySelector(targetSel) : btn.closest(".code-card").querySelector("pre code");
        if (!el) return;
        var text = el.innerText;
        navigator.clipboard.writeText(text).then(function () {
          var original = btn.textContent;
          btn.textContent = "Copied!";
          setTimeout(function () {
            btn.textContent = original;
          }, 1500);
        }).catch(function () {
          /* clipboard API unavailable — silently ignore */
        });
      });
    });
  }

  function initTabs() {
    document.querySelectorAll("[data-tabs]").forEach(function (group) {
      var buttons = group.querySelectorAll(".tab-btn");
      var panels = group.querySelectorAll(".tab-panel");
      buttons.forEach(function (btn) {
        btn.addEventListener("click", function () {
          var target = btn.getAttribute("data-tab");
          buttons.forEach(function (b) { b.classList.toggle("active", b === btn); });
          panels.forEach(function (p) { p.classList.toggle("active", p.getAttribute("data-panel") === target); });
        });
      });
    });
  }

  // The API sidebar is static HTML rebuilt on every page load: restore the groups
  // the reader had open, then open the ones holding the current page.
  function initApiTree() {
    var tree = document.querySelector(".api-tree");
    if (!tree) return;
    var KEY = "asge-api-tree";
    var groups = tree.querySelectorAll("details");
    var id = function (d) { return d.querySelector("summary a").getAttribute("href"); };
    var saved = [];
    try { saved = JSON.parse(sessionStorage.getItem(KEY) || "[]"); } catch (e) {}
    groups.forEach(function (d) { if (saved.indexOf(id(d)) !== -1) d.open = true; });

    var here = location.pathname.split("/").pop();
    tree.querySelectorAll("a").forEach(function (a) {
      if (a.getAttribute("href").split("#")[0].split("/").pop() !== here) return;
      a.classList.add("active");
      a.setAttribute("aria-current", "page");
      for (var d = a.closest("details"); d; d = d.parentElement.closest("details")) d.open = true;
    });

    groups.forEach(function (d) {
      d.addEventListener("toggle", function () {
        var open = [];
        groups.forEach(function (g) { if (g.open) open.push(id(g)); });
        try { sessionStorage.setItem(KEY, JSON.stringify(open)); } catch (e) {}
      });
    });
  }

  function highlightCode() {
    if (window.hljs) {
      document.querySelectorAll("pre code").forEach(function (block) {
        window.hljs.highlightElement(block);
      });
    }
  }

  document.addEventListener("DOMContentLoaded", function () {
    initThemeToggle();
    initNavToggle();
    initTocSpy();
    initCopyButtons();
    initTabs();
    initApiTree();
    highlightCode();
  });
})();
