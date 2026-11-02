#!/usr/bin/env python3
"""Renders docsite/site/engine/api/*.html from Doxygen's XML output (docsite/xml/).

Doxygen's job is reduced to "parse C++ and emit structured data" — every
page is rendered here with the exact same nav/CSS/card components as the
rest of the site (see docsite/_shared.py and docsite/generate_site.py),
instead of Doxygen's own themed HTML.

Pages land in site/engine/api/. Usage: run `doxygen Doxyfile` first (GENERATE_XML=YES in this repo's
Doxyfile), then `python docsite/generate_api.py`. Both docsite/xml/ (input)
and docsite/site/engine/api/ (output) are gitignored build artifacts, produced by CI
on every deploy — see .github/workflows/docs.yml.
"""
import html
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

from _shared import GITHUB_URL, ROOT, SITE, Page, render_page, write

XML_DIR = ROOT / "xml"
OUT = SITE / "engine" / "api"  # output directory; site/engine/api/*.html sits two levels below the site root

# ---------------------------------------------------------------------------
# loading
# ---------------------------------------------------------------------------

# Compound kinds we render our own page for. Doxygen also emits "file" and
# "dir" compounds (148 of them here) -- deliberately excluded: nobody
# browses to a file listing, and "defined at line N of <file>" already
# links straight to the real file on GitHub (see render_location).
PAGE_KINDS = {"class", "struct", "union", "interface", "concept", "namespace"}

SECTION_LABELS = {
    "public-type": "Public Types",
    "public-func": "Public Member Functions",
    "public-attrib": "Public Attributes",
    "public-static-func": "Static Public Member Functions",
    "public-static-attrib": "Static Public Attributes",
    "protected-type": "Protected Types",
    "protected-func": "Protected Member Functions",
    "protected-attrib": "Protected Attributes",
    "protected-static-func": "Static Protected Member Functions",
    "protected-static-attrib": "Static Protected Attributes",
    "private-type": "Private Types",
    "private-func": "Private Member Functions",
    "private-attrib": "Private Attributes",
    "private-static-func": "Static Private Member Functions",
    "private-static-attrib": "Static Private Attributes",
    "enum": "Enumerations",
    "typedef": "Typedefs",
    "func": "Functions",
    "var": "Variables",
    "friend": "Friends",
    "related": "Related",
}

KIND_TITLE = {
    "class": "Class",
    "struct": "Struct",
    "union": "Union",
    "interface": "Interface",
    "concept": "Concept",
    "namespace": "Namespace",
}


def load(refid: str) -> ET.Element:
    """Parses one compound's XML file, returning its <compounddef>."""
    tree = ET.parse(XML_DIR / f"{refid}.xml")
    return tree.getroot().find("compounddef")


def load_index():
    """Returns the list of (refid, kind, name) for every top-level compound
    worth a page: skips anonymous namespaces (per-.cpp-file implementation
    detail, not part of the public API surface -- a real name like
    asge::functools::_internal is a namespace we DO keep) and the empty
    "std" stub Doxygen synthesizes because our code references std:: types.
    """
    tree = ET.parse(XML_DIR / "index.xml")
    out = []
    for compound in tree.getroot().findall("compound"):
        kind = compound.get("kind")
        name = compound.findtext("name")
        if kind == "namespace" and (name == "std" or name.startswith("@")):
            continue
        out.append((compound.get("refid"), kind, name))
    return out


def build_refid_map(compounds):
    """refid -> url for every compound and every member declared in one.

    Members inherited into a derived class's <listofallmembers> are NOT
    recorded from there -- only from the compound that actually declares
    them -- so a cross-reference to an inherited member correctly lands on
    the base class's page, not a nonexistent anchor on the derived one.
    """
    refid_map = {}
    for refid, kind, _name in compounds:
        if kind not in PAGE_KINDS:
            continue
        refid_map[refid] = f"{refid}.html"
        try:
            cdef = load(refid)
        except FileNotFoundError:
            continue
        for sectiondef in cdef.findall("sectiondef"):
            for memberdef in sectiondef.findall("memberdef"):
                mid = memberdef.get("id")
                refid_map[mid] = f"{refid}.html#{mid}"
                if memberdef.get("kind") == "enum":
                    for enumvalue in memberdef.findall("enumvalue"):
                        refid_map[enumvalue.get("id")] = f"{refid}.html#{mid}"
    return refid_map


# ---------------------------------------------------------------------------
# rich-text (briefdescription/detaileddescription/type) rendering
# ---------------------------------------------------------------------------

REFID_MAP: dict[str, str] = {}  # populated by main() before any rendering
SIDEBAR_HTML: str = ""  # populated by main() before any rendering


def resolve_ref(refid: str) -> str | None:
    url = REFID_MAP.get(refid)
    return f"{url}" if url else None


def esc(text) -> str:
    return html.escape(text) if text else ""


def render_inline(node: ET.Element) -> str:
    """Renders a node's text + children as inline HTML (for <type>, <name>, ...)."""
    out = [esc(node.text)]
    for child in node:
        out.append(render_inline_tag(child))
        out.append(esc(child.tail))
    return "".join(out)


def render_inline_tag(node: ET.Element) -> str:
    tag = node.tag
    inner = render_inline(node)
    if tag == "ref":
        url = resolve_ref(node.get("refid"))
        return f'<a href="{url}">{inner}</a>' if url else inner
    if tag == "computeroutput":
        return f"<code>{inner}</code>"
    if tag in ("bold", "strong"):
        return f"<strong>{inner}</strong>"
    if tag in ("emphasis", "italic"):
        return f"<em>{inner}</em>"
    if tag == "sp":
        return "&nbsp;" if node.get("value") else " "
    if tag == "linebreak":
        return "<br/>"
    if tag == "ulink":
        return f'<a href="{esc(node.get("url"))}" target="_blank" rel="noopener">{inner}</a>'
    if tag == "anchor":
        return f'<a id="{esc(node.get("id"))}"></a>'
    # Unknown inline tag: keep its text/children rather than dropping content.
    return inner


CALLOUT_LABEL = {
    "warning": "Warning", "note": "Note", "attention": "Attention",
    "see": "See also", "return": "Returns", "author": "Author", "authors": "Authors",
    "version": "Version", "since": "Since", "date": "Date", "pre": "Precondition",
    "post": "Postcondition", "invariant": "Invariant", "remark": "Remark",
    "par": "", "rcs": "", "copyright": "Copyright", "todo": "Todo", "test": "Test",
}


def render_block(node: ET.Element) -> str:
    """Renders a node as block-level HTML (for description bodies)."""
    parts = []
    if node.text and node.text.strip():
        parts.append(f"<p>{esc(node.text)}</p>")
    for child in node:
        parts.append(render_block_tag(child))
        if child.tail and child.tail.strip():
            parts.append(f"<p>{esc(child.tail)}</p>")
    return "\n".join(p for p in parts if p)


def render_block_tag(node: ET.Element) -> str:
    tag = node.tag
    if tag == "para":
        inline = render_para_mixed(node)
        return inline
    if tag in ("itemizedlist", "orderedlist"):
        list_tag = "ul" if tag == "itemizedlist" else "ol"
        items = "".join(f"<li>{render_block(li)}</li>" for li in node.findall("listitem"))
        return f"<{list_tag}>{items}</{list_tag}>"
    if tag == "simplesect":
        kind = node.get("kind", "note")
        label = CALLOUT_LABEL.get(kind, kind.title())
        body = render_children_blocks(node)
        label_html = f'<div class="api-callout-label">{esc(label)}</div>' if label else ""
        return f'<div class="api-callout api-callout-{esc(kind)}">{label_html}<div>{body}</div></div>'
    if tag == "parameterlist":
        return render_parameterlist(node)
    if tag == "programlisting":
        return render_programlisting(node)
    if tag == "table":
        return render_table(node)
    if tag == "xrefsect":
        title = node.findtext("xreftitle") or ""
        desc = node.find("xrefdescription")
        body = render_children_blocks(desc) if desc is not None else ""
        return f'<div class="api-callout api-callout-todo"><div class="api-callout-label">{esc(title)}</div><div>{body}</div></div>'
    if tag == "heading":
        level = min(int(node.get("level", "3")) + 2, 6)
        return f"<h{level}>{render_inline(node)}</h{level}>"
    # Fallback: treat as an inline run so we never silently drop content.
    return f"<p>{render_inline(node)}</p>"


def render_children_blocks(node: ET.Element) -> str:
    return "\n".join(render_block_tag(c) if c.tag == "para" else render_block_tag(c) for c in node)


def render_para_mixed(node: ET.Element) -> str:
    """A <para> can contain both inline runs and block children (lists,
    simplesects, parameterlists, tables, programlistings). Split on the
    first block-level child so the inline text before it still becomes its
    own <p>, matching how Doxygen's own docs read."""
    BLOCK_TAGS = {"itemizedlist", "orderedlist", "simplesect", "parameterlist",
                  "programlisting", "table", "xrefsect", "heading"}
    buffer = [esc(node.text)]
    out = []

    def flush():
        joined = "".join(buffer).strip()
        buffer.clear()
        if joined:
            out.append(f"<p>{joined}</p>")

    for child in node:
        if child.tag in BLOCK_TAGS:
            flush()
            out.append(render_block_tag(child))
        else:
            buffer.append(render_inline_tag(child))
        if child.tail:
            buffer.append(esc(child.tail))
    flush()
    return "\n".join(out)


def render_parameterlist(node: ET.Element) -> str:
    kind = node.get("kind", "param")
    heading = {"param": "Parameters", "templateparam": "Template Parameters", "exception": "Exceptions"}.get(kind, kind.title())
    rows = []
    for item in node.findall("parameteritem"):
        names = [render_inline(n) for n in item.find("parameternamelist").findall("parametername")] if item.find("parameternamelist") is not None else []
        desc = item.find("parameterdescription")
        desc_html = render_children_blocks(desc) if desc is not None else ""
        rows.append(f"<tr><td><code>{', '.join(names)}</code></td><td>{desc_html}</td></tr>")
    return (
        f'<div class="api-callout api-callout-param"><div class="api-callout-label">{esc(heading)}</div>'
        f'<table class="api-param-table">{"".join(rows)}</table></div>'
    )


def render_programlisting(node: ET.Element) -> str:
    lines = []
    for codeline in node.findall("codeline"):
        lines.append(render_inline(codeline))
    code = "\n".join(lines)
    return f'<pre><code class="language-cpp">{code}</code></pre>'


def render_table(node: ET.Element) -> str:
    rows = []
    for row in node.findall("row"):
        cells = []
        for entry in row.findall("entry"):
            tag = "th" if entry.get("thead") == "yes" else "td"
            cells.append(f"<{tag}>{render_children_blocks(entry)}</{tag}>")
        rows.append(f"<tr>{''.join(cells)}</tr>")
    return f"<table>{''.join(rows)}</table>"


def render_desc(section: ET.Element | None) -> str:
    """Renders a <briefdescription> or <detaileddescription> element."""
    if section is None:
        return ""
    return render_children_blocks(section)


def has_content(section: ET.Element | None) -> bool:
    if section is None:
        return False
    return bool("".join(section.itertext()).strip())


def repo_relpath(file_path: str | None) -> str | None:
    if not file_path:
        return None
    file_path = file_path.replace("\\", "/")
    marker = "src/ASGE/"
    idx = file_path.find(marker)
    return file_path[idx:] if idx != -1 else None


def render_location(loc: ET.Element | None) -> str:
    if loc is None:
        return ""
    path = repo_relpath(loc.get("file"))
    if not path:
        return ""
    line = loc.get("line") or loc.get("bodystart")
    frag = f"#L{line}" if line else ""
    label = f"{path}:{line}" if line else path
    return (
        f'<a class="api-location" href="{GITHUB_URL}/blob/main/{path}{frag}" '
        f'target="_blank" rel="noopener">{esc(label)}</a>'
    )


def template_prefix(node: ET.Element) -> str:
    tpl = node.find("templateparamlist")
    if tpl is None or not len(tpl):
        return ""
    parts = []
    for param in tpl.findall("param"):
        type_html = render_inline(param.find("type")) if param.find("type") is not None else ""
        name = esc(param.findtext("declname") or "")
        default = param.find("defval")
        default_html = f" = {render_inline(default)}" if default is not None else ""
        parts.append(f"{type_html} {name}{default_html}".strip())
    return f"template&lt;{', '.join(parts)}&gt; "


# ---------------------------------------------------------------------------
# member rendering
# ---------------------------------------------------------------------------


def member_qualifiers(m: ET.Element) -> str:
    prefix = []
    if m.get("static") == "yes":
        prefix.append("static")
    if m.get("virt") in ("virtual", "pure-virtual"):
        prefix.append("virtual")
    if m.get("explicit") == "yes":
        prefix.append("explicit")
    if m.get("mutable") == "yes":
        prefix.append("mutable")
    return " ".join(prefix)


def render_function_sig(m: ET.Element) -> str:
    prefix = member_qualifiers(m)
    type_elem = m.find("type")
    has_type = type_elem is not None and (type_elem.text or len(type_elem))
    ret_type = render_inline(type_elem) if has_type else ""
    name = esc(m.findtext("name"))
    args = esc(m.findtext("argsstring") or "")
    tmpl = template_prefix(m)
    bits = [b for b in [tmpl.rstrip(), prefix, ret_type] if b]
    head = " ".join(bits)
    return f'{head}{" " if head else ""}<span class="api-member-name">{name}</span>{args}'


def render_variable_sig(m: ET.Element) -> str:
    prefix = member_qualifiers(m)
    type_html = render_inline(m.find("type"))
    name = esc(m.findtext("name"))
    args = esc(m.findtext("argsstring") or "")
    init = m.find("initializer")
    init_html = f" {render_inline(init)}" if init is not None else ""
    bits = [b for b in [prefix, type_html] if b]
    head = " ".join(bits)
    return f'{head} <span class="api-member-name">{name}</span>{args}{init_html}'


def render_typedef_sig(m: ET.Element) -> str:
    type_html = render_inline(m.find("type"))
    name = esc(m.findtext("name"))
    args = esc(m.findtext("argsstring") or "")
    return f'using <span class="api-member-name">{name}</span> = {type_html}{args}'


def render_enum_sig(m: ET.Element) -> str:
    type_elem = m.find("type")
    underlying = render_inline(type_elem) if type_elem is not None and (type_elem.text or len(type_elem)) else ""
    name = esc(m.findtext("name"))
    kind_word = "enum class" if m.get("strong") == "yes" else "enum"
    suffix = f" : {underlying}" if underlying else ""
    return f'{kind_word} <span class="api-member-name">{name}</span>{suffix}'


def render_enum_values(m: ET.Element) -> str:
    values = m.findall("enumvalue")
    any_desc = any(has_content(v.find("briefdescription")) for v in values)
    rows = []
    for v in values:
        vid = v.get("id")
        name = esc(v.findtext("name"))
        brief = render_desc(v.find("briefdescription"))
        rows.append(
            f'<div class="api-enum-value" id="{vid}"><code>{name}</code>'
            f'<div class="api-member-body">{brief}</div></div>'
        )
    cls = "api-enum-values has-descriptions" if any_desc else "api-enum-values"
    return f'<div class="{cls}">{"".join(rows)}</div>' if rows else ""


SIG_RENDERERS = {
    "function": render_function_sig,
    "variable": render_variable_sig,
    "typedef": render_typedef_sig,
    "enum": render_enum_sig,
}


def render_member(m: ET.Element) -> str:
    kind = m.get("kind")
    mid = m.get("id")
    prot = m.get("prot")

    sig_fn = SIG_RENDERERS.get(kind)
    if sig_fn:
        sig_html = sig_fn(m)
    else:
        # Generic fallback (friend, define, ...): never silently drop a member.
        name = esc(m.findtext("name") or "")
        sig_html = f'<span class="api-kind-tag">{esc(kind)}</span> <span class="api-member-name">{name}</span>'

    reimplements = m.find("reimplements")
    reimpl_html = ""
    if reimplements is not None:
        url = resolve_ref(reimplements.get("refid"))
        if url:
            reimpl_html = f'<div class="api-member-note">Overrides <a href="{url}">{esc(reimplements.text)}</a></div>'

    body_parts = []
    brief = m.find("briefdescription")
    detailed = m.find("detaileddescription")
    if has_content(brief):
        body_parts.append(render_desc(brief))
    if has_content(detailed):
        body_parts.append(render_desc(detailed))
    if kind == "enum":
        body_parts.append(render_enum_values(m))
    body_parts.append(reimpl_html)
    loc_html = render_location(m.find("location"))
    if loc_html:
        body_parts.append(f'<div class="api-location-line">Defined at {loc_html}</div>')

    body_html = "".join(p for p in body_parts if p)
    prot_tag = f'<span class="tag api-prot-{esc(prot)}">{esc(prot)}</span>' if prot and prot != "public" else ""

    return f"""<div class="api-member" id="{mid}">
  <div class="api-member-sig"><a class="api-anchor-link" href="#{mid}">#</a><code>{sig_html}</code>{prot_tag}</div>
  <div class="api-member-body">{body_html}</div>
</div>"""


def render_sections(cdef: ET.Element) -> str:
    out = []
    for sectiondef in cdef.findall("sectiondef"):
        kind = sectiondef.get("kind")
        label = SECTION_LABELS.get(kind, kind.replace("-", " ").title())
        members = sectiondef.findall("memberdef")
        if not members:
            continue
        member_html = "\n".join(render_member(m) for m in members)
        out.append(f'<h3 class="api-section-title">{esc(label)}</h3>\n{member_html}')
    return "\n".join(out)


# ---------------------------------------------------------------------------
# Static SVG diagrams from <inheritancegraph>/<collaborationgraph>
#
# Doxygen emits tiny graphs (2 to 8 nodes), so a layered layout is enough: bases
# and owners sit above, derived classes and members below, edges are orthogonal
# elbows. Colours come from the `.diagram .d-*` classes in style.css, so the
# figures follow the light/dark toggle and need no JavaScript.
# ---------------------------------------------------------------------------

_GRAPH_NODE_H = 44
_GRAPH_GAP_X = 32
_GRAPH_GAP_Y = 72
_GRAPH_PAD = 16


def sanitize_label(label: str) -> str:
    """Strips <template, args> from a compound label -- bracket-depth aware, since a
    naive `re.sub(r"<.*", "", label)` deletes everything after the FIRST '<',
    including any trailing "::Nested" name after the argument list closes."""
    out = []
    depth = 0
    for ch in label:
        if ch == "<":
            depth += 1
        elif ch == ">":
            if depth > 0:
                depth -= 1
        elif depth == 0:
            out.append(ch)
    return "".join(out).strip()


def _short_label(label: str) -> str:
    """The unqualified class name: the namespace is already in the page title."""
    full = sanitize_label(label)
    return full.rsplit("::", 1)[-1] or full


def _rounded_path(points: list[tuple[float, float]], r: float = 8) -> str:
    """SVG path through orthogonal points, with rounded corners of radius up to r."""
    pts = [points[0]]
    for p in points[1:]:
        if p != pts[-1]:
            pts.append(p)
    d = f"M{pts[0][0]:g} {pts[0][1]:g}"
    for i in range(1, len(pts) - 1):
        (px, py), (cx, cy), (nx, ny) = pts[i - 1], pts[i], pts[i + 1]
        din = abs(cx - px) + abs(cy - py)
        dout = abs(nx - cx) + abs(ny - cy)
        rr = min(r, din / 2, dout / 2)
        ex = cx + (px - cx) / din * rr if px != cx else cx
        ey = cy + (py - cy) / din * rr if py != cy else cy
        fx = cx + (nx - cx) / dout * rr if nx != cx else cx
        fy = cy + (ny - cy) / dout * rr if ny != cy else cy
        d += f"L{ex:g} {ey:g}Q{cx:g} {cy:g} {fx:g} {fy:g}"
    return d + f"L{pts[-1][0]:g} {pts[-1][1]:g}"


def render_graph_svg(graph_elem: ET.Element | None, focal_refid: str, uid: str, heading: str) -> str:
    if graph_elem is None:
        return ""
    nodes = list(graph_elem.findall("node"))
    if len(nodes) < 2:
        return ""
    ids = [n.get("id") for n in nodes]
    info = {}
    for n in nodes:
        link = n.find("link")
        refid = link.get("refid") if link is not None else None
        label = _short_label(n.findtext("label") or "")
        info[n.get("id")] = {
            "label": label,
            "full": sanitize_label(n.findtext("label") or ""),
            "url": resolve_ref(refid) if refid else None,
            "focal": refid == focal_refid,
        }
    if not any(v["focal"] for v in info.values()):
        info[ids[0]]["focal"] = True

    # (upper, lower, kind, label): bases and owners are "upper"
    edges = []
    for n in nodes:
        for child in n.findall("childnode"):
            c = child.get("refid")
            if c not in info:
                continue
            if (child.get("relation") or "").endswith("inheritance"):
                edges.append((c, n.get("id"), "inherit", ""))
            else:
                labels = ", ".join(e.text for e in child.findall("edgelabel") if e.text)
                edges.append((n.get("id"), c, "use", labels))
    if not edges:
        return ""

    layer = {i: 0 for i in ids}
    for _ in range(len(ids)):
        for up, low, _kind, _label in edges:
            layer[low] = max(layer[low], layer[up] + 1)
    nlayers = max(layer.values()) + 1

    for i in ids:
        info[i]["w"] = max(112, int(8.3 * len(info[i]["label"])) + 32) // 4 * 4
    rows = [[i for i in ids if layer[i] == k] for k in range(nlayers)]
    row_w = [sum(info[i]["w"] for i in r) + _GRAPH_GAP_X * (len(r) - 1) for r in rows]
    width = max(max(row_w) + 2 * _GRAPH_PAD, 240)
    # each outgoing edge of one box gets its own horizontal level, so the gap grows with the fan-out
    max_out = max(sum(1 for e in edges if e[0] == i) for i in ids)
    gap_y = max(_GRAPH_GAP_Y, 44 + 12 * max_out)
    height = 2 * _GRAPH_PAD + nlayers * _GRAPH_NODE_H + (nlayers - 1) * gap_y
    for k, r in enumerate(rows):
        x = (width - row_w[k]) / 2
        for i in r:
            info[i]["x"] = round(x)
            info[i]["y"] = _GRAPH_PAD + k * (_GRAPH_NODE_H + gap_y)
            x += info[i]["w"] + _GRAPH_GAP_X

    def cx(i):
        return info[i]["x"] + info[i]["w"] / 2

    # fan attach points: each edge on a box edge gets its own x, ordered left to right
    out_edges = {i: sorted((e for e in edges if e[0] == i), key=lambda e: cx(e[1])) for i in ids}
    in_edges = {i: sorted((e for e in edges if e[1] == i), key=lambda e: cx(e[0])) for i in ids}

    def attach(i, group, e):
        k, n = group.index(e), len(group)
        return round(info[i]["x"] + info[i]["w"] * (k + 1) / (n + 1))

    # Horizontal levels that cannot cross: right-going edges stack with the rightmost
    # attach point highest, left-going edges mirror that, so no edge crosses a sibling's drop.
    level = {}
    for i, outs in out_edges.items():
        sxs = {e: attach(i, outs, e) for e in outs}
        txs = {e: attach(e[1], in_edges[e[1]], e) for e in outs}
        left = sorted((e for e in outs if txs[e] < sxs[e]), key=lambda e: sxs[e])
        right = sorted((e for e in outs if txs[e] > sxs[e]), key=lambda e: -sxs[e])
        for n_, e in enumerate(left):
            level[e] = n_
        for n_, e in enumerate(right):
            level[e] = n_
        for e in outs:
            level.setdefault(e, 0)

    paths, labels_svg, desc = [], [], []
    min_x, max_x = 0, width
    for up, low, kind, label in edges:
        sx = attach(up, out_edges[up], (up, low, kind, label))
        tx = attach(low, in_edges[low], (up, low, kind, label))
        y1 = info[up]["y"] + _GRAPH_NODE_H
        y2 = info[low]["y"]
        ymid = y1 + 20 + level[(up, low, kind, label)] * 12
        if layer[low] > layer[up] + 1:  # skips a layer: detour around the left margin
            pts = [(sx, y1), (sx, ymid), (_GRAPH_PAD / 2, ymid), (_GRAPH_PAD / 2, y2 - 28), (tx, y2 - 28), (tx, y2)]
        else:
            pts = [(sx, y1), (sx, ymid), (tx, ymid), (tx, y2)]
        if kind == "inherit":
            pts.reverse()  # the arrowhead sits on the base class
            cls, marker = "d-arrow", f"{uid}-open"
            desc.append(f"{info[low]['label']} inherits from {info[up]['label']}")
        else:
            cls, marker = "d-arrow", f"{uid}-arrow"
            desc.append(f"{info[up]['label']} uses {info[low]['label']}" + (f" as {label}" if label else ""))
        paths.append(f'<path class="{cls}" d="{_rounded_path(pts)}" marker-end="url(#{marker})"/>')
        if label:
            text_w = 6.6 * len(label)
            # with several edges into one box, the leftmost label goes left of its arrow, the rest right
            left = len(in_edges[low]) > 1 and in_edges[low].index((up, low, kind, label)) == 0
            if left:
                labels_svg.append(f'<text x="{tx - 8}" y="{y2 - 6}" text-anchor="end" class="d-edge">{esc(label)}</text>')
                min_x = min(min_x, tx - 8 - text_w - _GRAPH_PAD)
            else:
                labels_svg.append(f'<text x="{tx + 8}" y="{y2 - 6}" class="d-edge">{esc(label)}</text>')
                max_x = max(max_x, tx + 8 + text_w + _GRAPH_PAD)
    width = int(max_x - min_x)

    boxes = []
    for i in ids:
        v = info[i]
        cls = "d-node d-focal" if v["focal"] else "d-node"
        cxx, cyy = v["x"] + v["w"] / 2, v["y"] + _GRAPH_NODE_H / 2
        g = (f'<rect x="{v["x"]}" y="{v["y"]}" width="{v["w"]}" height="{_GRAPH_NODE_H}" rx="6" class="{cls}"/>'
             f'<text x="{cxx:g}" y="{cyy + 5:g}" text-anchor="middle" class="d-name">{esc(v["label"])}</text>')
        if v["url"] and not v["focal"]:
            g = f'<a href="{v["url"]}"><title>{esc(v["full"])}</title>{g}</a>'
        else:
            g = f'<g><title>{esc(v["full"])}</title>{g}</g>'
        boxes.append(g)

    return (
        f'<figure class="diagram api-diagram"><h4>{heading}</h4><div class="diagram-scroll">'
        f'<svg viewBox="{int(min_x)} 0 {width} {height}" style="width:{width}px;max-width:100%" role="img" '
        f'aria-labelledby="{uid}-t {uid}-d"><title id="{uid}-t">{heading} graph</title>'
        f'<desc id="{uid}-d">{esc("; ".join(desc))}.</desc>'
        f'<defs><marker id="{uid}-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto">'
        f'<path d="M0 0L10 5L0 10z" class="d-head"/></marker>'
        f'<marker id="{uid}-open" viewBox="0 0 12 12" refX="11" refY="6" markerWidth="10" markerHeight="10" orient="auto">'
        f'<path d="M1 1L11 6L1 11z" class="d-head-open"/></marker></defs>'
        + "".join(paths) + "".join(boxes) + "".join(labels_svg)
        + "</svg></div></figure>"
    )


# ---------------------------------------------------------------------------
# page: one class/struct/union/concept
# ---------------------------------------------------------------------------


def render_compound_page(refid: str) -> str:
    cdef = load(refid)
    kind = cdef.get("kind")
    name = cdef.findtext("compoundname") or refid
    tmpl = template_prefix(cdef)
    badges = []
    if cdef.get("abstract") == "yes":
        badges.append("abstract")
    if cdef.get("final") == "yes":
        badges.append("final")
    badge_html = "".join(f'<span class="tag">{b}</span>' for b in badges)

    title = f"{tmpl}{esc(name)}"
    kicker = KIND_TITLE.get(kind, kind.title())

    includes = cdef.find("includes")
    includes_html = (
        f'<div class="code-card"><pre><code class="language-cpp">#include &lt;{esc(includes.text)}&gt;</code></pre></div>'
        if includes is not None and includes.text else ""
    )

    bases = cdef.findall("basecompoundref")
    bases_html = ""
    if bases:
        links = []
        for b in bases:
            refid_b = b.get("refid")
            url = resolve_ref(refid_b) if refid_b else None
            label = esc(b.text or "")
            links.append(f'<a href="{url}">{label}</a>' if url else label)
        bases_html = f'<p class="api-inherits">Inherits from {", ".join(links)}.</p>'

    deriveds = cdef.findall("derivedcompoundref")
    deriveds_html = ""
    if deriveds:
        links = []
        for d in deriveds:
            refid_d = d.get("refid")
            url = resolve_ref(refid_d) if refid_d else None
            label = esc(d.text or "")
            links.append(f'<a href="{url}">{label}</a>' if url else label)
        deriveds_html = f'<p class="api-inherits">Inherited by {", ".join(links)}.</p>'

    diagrams_html = (
        render_graph_svg(cdef.find("inheritancegraph"), refid, "inh", "Inheritance")
        + render_graph_svg(cdef.find("collaborationgraph"), refid, "col", "Collaboration")
    )

    brief_html = render_desc(cdef.find("briefdescription"))
    detailed_html = render_desc(cdef.find("detaileddescription"))
    loc_html = render_location(cdef.find("location"))

    sections_html = render_sections(cdef)

    body = f"""
    <a class="back-link" href="index.html">&larr; API Reference</a>
    <span class="kicker">{esc(kicker)}</span>
    <h1 class="api-title">{title} {badge_html}</h1>
    {f'<div class="lede">{brief_html}</div>' if brief_html else ""}
    {includes_html}
    {bases_html}
    {deriveds_html}
    {diagrams_html}
    {f'<div class="api-detailed">{detailed_html}</div>' if detailed_html else ""}
    {sections_html}
    {f'<p class="api-location-line">Defined at {loc_html}</p>' if loc_html else ""}
"""
    return render_page(
        page=api_page(refid, name, strip_tags(brief_html) or f"{kicker} {name} — ASGE API reference."),
        body=body, sidebar=SIDEBAR_HTML,
    )


# ---------------------------------------------------------------------------
# page: one namespace
# ---------------------------------------------------------------------------


def render_namespace_page(refid: str) -> str:
    cdef = load(refid)
    name = cdef.findtext("compoundname") or refid

    inner_ns = cdef.findall("innernamespace")
    inner_ns_html = ""
    if inner_ns:
        cards = "".join(
            f'<a class="card" href="{resolve_ref(n.get("refid"))}"><h3>{esc(n.text)}</h3><p>Namespace</p></a>'
            for n in inner_ns if resolve_ref(n.get("refid"))
        )
        inner_ns_html = f'<h3 class="api-section-title">Namespaces</h3><div class="grid">{cards}</div>'

    inner_classes = cdef.findall("innerclass")
    inner_classes_html = ""
    if inner_classes:
        cards = "".join(
            f'<a class="card" href="{resolve_ref(c.get("refid"))}"><h3>{esc(c.text.rsplit("::", 1)[-1])}</h3>'
            f'<p>{esc(REFID_KIND.get(c.get("refid"), "Class"))}</p></a>'
            for c in inner_classes if resolve_ref(c.get("refid"))
        )
        inner_classes_html = f'<h3 class="api-section-title">Classes &amp; Structs</h3><div class="grid">{cards}</div>'

    brief_html = render_desc(cdef.find("briefdescription"))
    detailed_html = render_desc(cdef.find("detaileddescription"))
    sections_html = render_sections(cdef)

    body = f"""
    <a class="back-link" href="index.html">&larr; API Reference</a>
    <span class="kicker">Namespace</span>
    <h1 class="api-title">{esc(name)}</h1>
    {f'<div class="lede">{brief_html}</div>' if brief_html else ""}
    {f'<div class="api-detailed">{detailed_html}</div>' if detailed_html else ""}
    {inner_ns_html}
    {inner_classes_html}
    {sections_html}
"""
    return render_page(
        page=api_page(refid, f"{name} (namespace)", strip_tags(brief_html) or f"The {name} namespace — ASGE API reference."),
        body=body, sidebar=SIDEBAR_HTML,
    )


def strip_tags(s: str) -> str:
    return re.sub(r"<[^>]+>", "", s or "").strip()[:200]


def api_page(refid: str, title: str, summary: str) -> Page:
    """The (unregistered) Page for one generated API file; its sidebar is the namespace tree."""
    return Page(f"engine/api/{refid}.html", title, "api", "", summary)


# ---------------------------------------------------------------------------
# sidebar (namespace tree, every API page)
# ---------------------------------------------------------------------------

REFID_KIND: dict[str, str] = {}  # refid -> "Class"/"Struct"/... for card subtitles


def build_namespace_tree(compounds):
    """Returns the top-level namespace refids in declaration order, plus a
    refid -> compounddef cache so callers don't re-parse namespace files."""
    namespaces = {refid: name for refid, kind, name in compounds if kind == "namespace"}
    ns_cdefs = {refid: load(refid) for refid in namespaces}
    child_of = {}
    for refid, cdef in ns_cdefs.items():
        for inner in cdef.findall("innernamespace"):
            child_of[inner.get("refid")] = refid
    roots = [refid for refid in namespaces if refid not in child_of]
    roots.sort(key=lambda r: namespaces[r])
    return roots, ns_cdefs, namespaces


def render_sidebar_tree(refid: str, ns_cdefs, namespaces, depth: int = 0) -> str:
    cdef = ns_cdefs[refid]
    short_name = namespaces[refid].rsplit("::", 1)[-1]
    url = resolve_ref(refid)

    classes = sorted(
        cdef.findall("innerclass"),
        key=lambda c: (c.text or "").rsplit("::", 1)[-1],
    )
    class_items = "".join(
        f'<li><a href="{resolve_ref(c.get("refid"))}">{esc((c.text or "").rsplit("::", 1)[-1])}</a></li>'
        for c in classes if resolve_ref(c.get("refid"))
    )

    child_ns = sorted(cdef.findall("innernamespace"), key=lambda n: n.text or "")
    child_html = "".join(
        render_sidebar_tree(n.get("refid"), ns_cdefs, namespaces, depth + 1)
        for n in child_ns if n.get("refid") in ns_cdefs
    )

    inner = f'<ul>{class_items}</ul>' if class_items else ""
    return f"""<details{' open' if depth == 0 else ''}>
  <summary><a href="{url}">{esc(short_name)}</a></summary>
  {inner}
  {child_html}
</details>"""


def build_sidebar(compounds) -> str:
    roots, ns_cdefs, namespaces = build_namespace_tree(compounds)
    tree_html = "".join(render_sidebar_tree(r, ns_cdefs, namespaces) for r in roots)
    return f"""<nav class="api-tree" aria-label="API namespaces">
  <h4>API reference</h4>
  {tree_html}
</nav>"""


# ---------------------------------------------------------------------------
# page: API index (the @mainpage content, or a generated fallback)
# ---------------------------------------------------------------------------


def render_index_page(compounds) -> str:
    mainpage_file = XML_DIR / "indexpage.xml"
    if mainpage_file.exists():
        tree = ET.parse(mainpage_file)
        cdef = tree.getroot().find("compounddef")
        title = cdef.findtext("title") or "API Reference"
        content_html = render_desc(cdef.find("detaileddescription"))
    else:
        title = "API Reference"
        content_html = "<p>Browse the API by namespace using the sidebar.</p>"

    body = f"""
    <h1 class="api-title">{esc(title)}</h1>
    <div class="api-detailed">{content_html}</div>
"""
    return render_page(
        page=Page(
            "engine/api/index.html", "API Reference", "api", "",
            "ASGE API reference — generated from the doc comments on every class, struct, function and namespace under src/ASGE/.",
        ),
        body=body, sidebar=SIDEBAR_HTML,
    )


# ---------------------------------------------------------------------------
# client-side search index
# ---------------------------------------------------------------------------


def build_search_index(compounds) -> list[dict]:
    entries = []
    for refid, kind, name in compounds:
        if kind not in PAGE_KINDS:
            continue
        url = resolve_ref(refid)
        if url:
            entries.append({
                "title": name, "url": f"engine/api/{url}", "section": "API",
                "text": KIND_TITLE.get(kind, kind.title()),
            })
    entries.sort(key=lambda e: e["title"])
    return entries


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def main():
    global REFID_MAP, SIDEBAR_HTML, REFID_KIND

    compounds = load_index()
    REFID_MAP = build_refid_map(compounds)
    REFID_KIND = {refid: KIND_TITLE.get(kind, kind.title()) for refid, kind, _ in compounds}
    SIDEBAR_HTML = build_sidebar(compounds)

    write(OUT / "index.html", render_index_page(compounds))

    for refid, kind, _name in compounds:
        if kind == "namespace":
            write(OUT / f"{refid}.html", render_namespace_page(refid))
        elif kind in PAGE_KINDS:
            write(OUT / f"{refid}.html", render_compound_page(refid))

    search_index = build_search_index(compounds)
    (OUT / "search-index.json").write_text(
        json.dumps(search_index), encoding="utf-8"
    )
    print(f"wrote engine/api/search-index.json ({len(search_index)} entries)")


if __name__ == "__main__":
    main()
