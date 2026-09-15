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
    var header = document.querySelector(".site-header");
    if (!toggle || !header) return;
    toggle.addEventListener("click", function () {
      header.classList.toggle("nav-open");
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
    initCopyButtons();
    initTabs();
    highlightCode();
  });
})();
