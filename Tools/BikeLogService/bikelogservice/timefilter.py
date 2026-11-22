"""The control of the map (period, colouring of the tiles), served as GET /api/v1/map/timefilter.js.

One script for both maps: the /map page and BRouter-Web, whose config.js loads it from here (so
brouter-web needs no patch and no rebuild). It puts a control on the map (button, slider for start
and end in months, two date fields for exact days, presets) and appends from/to to the URL of
every BikeLog tile. The same box switches the tile layers between cluster and frequency
(colour=rides, on a dark base map rides-dark) and has their legend.

  window.BikeLogPeriod.attach(L, map, options)            the /map page calls it ({always, change,
                                                          position}); in BRouter-Web the script
                                                          finds the map itself
  window.BikeLogPeriod.query()                            "from=...&to=...", "" = all rides
  window.BikeLogPeriod.link()                             query for a link to the other map
  window.BikeLogPeriod.colour()                           'cluster' or 'rides'

Period and colouring are kept per origin in localStorage; ?bikelogfrom=&bikelogto= and
?bikelogcolour= in the page's URL win once and are then taken out of it.

Other sites can load a script, so the only thing of the rides in it is the month of the first
one (where the slider begins); the figures stay in GET /api/v1/map, same origin only.
"""

from __future__ import annotations

import datetime
import json

from bikelog import explorer

_JS = r"""(function () {
'use strict';
if (window.BikeLogPeriod) return;
var FIRST = __FIRST__;                  // 'YYYY-MM' of the first ride, null without one
var TILES = '/api/v1/map/tiles/';
var KEY = 'bikelog.map.period';
var DAY = /^\d{4}-\d\d-\d\d$/;
var range = {from: '', to: ''};         // days, both included; '' = open
var COLOUR_KEY = 'bikelog.map.colour';
var LEGEND = __LEGEND__;
var colour = 'cluster';                 // of the tile layers: 'cluster' | 'rides'
var dark = false;                       // a dark base map is on

function valid(day) {
  if (!DAY.test(day || '')) return '';
  var d = new Date(day + 'T00:00:00Z');
  return isFinite(d) && d.toISOString().slice(0, 10) === day && day > '1900' ? day : '';
}
function store() {
  try {
    localStorage.setItem(KEY, JSON.stringify(range));
    localStorage.setItem(COLOUR_KEY, colour);
  } catch (e) { /* private mode */ }
}
function load() {
  var params = new URLSearchParams(location.search);
  var linked = params.has('bikelogfrom') || params.has('bikelogto');
  var from = '', to = '';
  if (linked) {
    from = params.get('bikelogfrom'); to = params.get('bikelogto');
  } else {
    try {
      var saved = JSON.parse(localStorage.getItem(KEY) || '{}');
      from = saved.from; to = saved.to;
    } catch (e) { /* nothing usable saved */ }
  }
  from = valid(from); to = valid(to);
  if (from && to && from > to) from = to = '';
  range = {from: from, to: to};
  var coloured = params.has('bikelogcolour');
  try {
    var wanted = coloured ? params.get('bikelogcolour') : localStorage.getItem(COLOUR_KEY);
    if (wanted === 'rides') colour = wanted;
  } catch (e) { /* no storage */ }
  if (linked || coloured) {
    store();
    ['bikelogfrom', 'bikelogto', 'bikelogcolour'].forEach(function (name) { params.delete(name); });
    var rest = params.toString();
    history.replaceState(history.state, '', location.pathname + (rest ? '?' + rest : '') + location.hash);
  }
}
load();

function query() {
  var parts = [];
  if (range.from) parts.push('from=' + range.from);
  if (range.to) parts.push('to=' + range.to);
  return parts.join('&');
}
function ours(layer) { return typeof layer._url === 'string' && layer._url.indexOf(TILES) >= 0; }
function coloured(layer) { return ours(layer) && layer._url.indexOf(TILES + 'heat/') < 0; }
function tileQuery(layer) {
  var parts = query() ? [query()] : [];
  if (colour === 'rides' && coloured(layer)) parts.push('colour=' + (dark ? 'rides-dark' : 'rides'));
  return parts.join('&');
}
function darkBase(layer) {
  return typeof layer._url === 'string' && /dark_all|dark_nolabels|dark-matter/.test(layer._url);
}

var patched = false;
function patch(L) {
  if (patched) return;
  patched = true;
  var original = L.TileLayer.prototype.getTileUrl;
  L.TileLayer.prototype.getTileUrl = function () {
    var url = original.apply(this, arguments);
    if (!ours(this)) return url;
    if (this._map) {                    // the base map may have changed with this very layer
      var isDark = false;
      this._map.eachLayer(function (layer) { if (darkBase(layer)) isDark = true; });
      dark = isDark;
    }
    var q = tileQuery(this);
    return q ? url + (url.indexOf('?') < 0 ? '?' : '&') + q : url;
  };
}

function pad(n) { return (n < 10 ? '0' : '') + n; }
function iso(d) { return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()); }
function german(day) { return day.slice(8, 10) + '.' + day.slice(5, 7) + '.' + day.slice(0, 4); }
function title(r) {
  if (!r.from && !r.to) return 'alle Fahrten';
  if (!r.to) return 'ab ' + german(r.from);
  if (!r.from) return 'bis ' + german(r.to);
  var year = r.from.slice(0, 4);
  if (r.from === year + '-01-01' && r.to === year + '-12-31') return year;
  return r.from === r.to ? german(r.from) : german(r.from) + ' – ' + german(r.to);
}

var now = new Date();
var first = FIRST ? new Date(+FIRST.slice(0, 4), +FIRST.slice(5, 7) - 1, 1) : now;
var months = Math.max(1, (now.getFullYear() - first.getFullYear()) * 12 + now.getMonth() - first.getMonth());
function monthStart(i) { return iso(new Date(first.getFullYear(), first.getMonth() + i, 1)); }
function monthEnd(i) { return iso(new Date(first.getFullYear(), first.getMonth() + i + 1, 0)); }
function monthOf(day) {
  var i = (+day.slice(0, 4) - first.getFullYear()) * 12 + +day.slice(5, 7) - 1 - first.getMonth();
  return Math.max(0, Math.min(months, i));
}

function presets() {
  var y = now.getFullYear(), m = now.getMonth(), d = now.getDate();
  var out = [['alle Fahrten', '', ''],
             ['letzte 30 Tage', iso(new Date(y, m, d - 29)), ''],
             ['letzte 12 Monate', iso(new Date(y - 1, m, d + 1)), '']];
  for (var year = y; year >= first.getFullYear(); year--) out.push(['' + year, year + '-01-01', year + '-12-31']);
  return out;
}

var CSS = [
  '.blp{background:#161B1F;color:#E7E2D6;border:1.5px solid #3A362E;border-radius:14px;',
  ' font:13px/1.4 "IBM Plex Sans",system-ui,sans-serif;max-width:calc(100vw - 64px);cursor:default}',
  '.blp [hidden],.blp[hidden]{display:none!important}',
  '.blp button.blp-head{display:block;width:100%;margin:0;padding:8px 14px;border:0;border-radius:12px;',
  ' background:none;color:inherit;font:inherit;text-align:left;white-space:nowrap;cursor:pointer;box-shadow:none}',
  '.blp-head small{color:#9BA097;font:500 11px "IBM Plex Mono",ui-monospace,monospace;letter-spacing:.08em;',
  ' text-transform:uppercase;margin-right:8px}',
  '.blp.blp-on{border-color:#CBA36B}.blp.blp-on .blp-head b{color:#CBA36B}',
  '.blp-body{width:340px;max-width:100%;max-height:50vh;overflow:auto;box-sizing:border-box;padding:2px 14px 12px}',
  '.blp-head span{display:block}',
  '.blp-colour{border-top:1.5px solid #3A362E;margin-top:10px;padding-top:10px}',
  '.blp-modes{display:flex;gap:6px}',
  '.blp button.blp-mode{flex:1;margin:0;padding:4px 8px;border:1.5px solid #3A362E;border-radius:999px;',
  ' background:#1E252B;color:inherit;font:inherit;cursor:pointer;box-shadow:none}',
  '.blp button.blp-mode[aria-pressed=true]{background:#CBA36B;border-color:#CBA36B;color:#161B1F}',
  '.blp-legend{display:flex;gap:4px;margin-top:8px}',
  '.blp-legend span{flex:1;font-size:11px;line-height:1.2;text-align:center;color:#E7E2D6}',
  '.blp-legend i{display:block;box-sizing:border-box;height:12px;margin-bottom:3px;border-radius:3px}',
  '.blp-what{margin-top:4px;color:#9BA097;font-size:11px}',
  '.blp-slide{position:relative;height:38px;touch-action:none}',
  '.blp-track,.blp-fill{position:absolute;top:17px;height:4px;border-radius:2px;background:#332F28}',
  '.blp-track{left:12px;right:12px}.blp-fill{background:#CBA36B;top:0}',
  '.blp input[type=range]{-webkit-appearance:none;appearance:none;position:absolute;left:0;top:0;width:100%;',
  ' height:38px;margin:0;padding:0;background:none;border:0;pointer-events:none;touch-action:none}',
  '.blp input[type=range]::-webkit-slider-runnable-track{background:none;border:0;height:38px}',
  '.blp input[type=range]::-moz-range-track{background:none;border:0}',
  '.blp input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;pointer-events:auto;box-sizing:border-box;',
  ' width:24px;height:24px;margin-top:7px;border-radius:50%;background:#CBA36B;border:3px solid #161B1F;cursor:pointer}',
  '.blp input[type=range]::-moz-range-thumb{pointer-events:auto;box-sizing:border-box;',
  ' width:24px;height:24px;border-radius:50%;background:#CBA36B;border:3px solid #161B1F;cursor:pointer}',
  '.blp input[type=range]:focus-visible{outline:1px solid #CBA36B;border-radius:8px}',
  '.blp-days{display:flex;gap:6px;align-items:center;margin:2px 0 8px}',
  '.blp input[type=date],.blp select{box-sizing:border-box;min-width:0;flex:1;margin:0;padding:4px 6px;',
  ' height:auto;color:#E7E2D6;background:#1E252B;border:1.5px solid #3A362E;border-radius:8px;',
  ' font:inherit;color-scheme:dark}',
  '.blp select{width:100%}'
].join('\n');

function el(tag, cls, parent, attrs) {
  var e = document.createElement(tag);
  if (cls) e.className = cls;
  for (var k in attrs || {}) e.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(e);
  return e;
}

function control(L, map, options) {
  var box = el('div', 'blp leaflet-control');
  var head = el('button', 'blp-head', box, {type: 'button', 'aria-expanded': 'false'});
  var periodLine = el('span', '', head), colourLine = el('span', '', head);
  el('small', '', periodLine).textContent = 'Zeitraum';
  var name = el('b', '', periodLine);
  el('small', '', colourLine).textContent = 'Kacheln';
  var colourName = el('span', '', colourLine);
  colourName.style.display = 'inline';
  var body = el('div', 'blp-body', box);
  body.hidden = true;
  var slide = el('div', 'blp-slide', body);
  var fill = el('div', 'blp-fill', el('div', 'blp-track', slide));
  var attrs = {type: 'range', min: 0, max: months, step: 1};
  var start = el('input', '', slide, attrs), end = el('input', '', slide, attrs);
  start.setAttribute('aria-label', 'Beginn (Monat)');
  end.setAttribute('aria-label', 'Ende (Monat)');
  var days = el('div', 'blp-days', body);
  var from = el('input', '', days, {type: 'date', 'aria-label': 'erster Tag'});
  el('span', '', days).textContent = '–';
  var to = el('input', '', days, {type: 'date', 'aria-label': 'letzter Tag'});
  var pick = el('select', '', body, {'aria-label': 'Vorgaben'});
  var list = presets();
  list.forEach(function (p, i) { el('option', '', pick, {value: i}).textContent = p[0]; });
  el('option', '', pick, {value: 'own', disabled: ''}).textContent = 'eigener Zeitraum';
  var colours = el('div', 'blp-colour', body), modes = el('div', 'blp-modes', colours);
  var buttons = [['cluster', 'Cluster'], ['rides', 'Häufigkeit']].map(function (m) {
    var button = el('button', 'blp-mode', modes, {type: 'button', 'data-colour': m[0]});
    button.textContent = m[1];
    button.addEventListener('click', function () {
      if (colour === m[0]) return;
      colour = m[0];
      store();
      showColour();
      redrawTiles();
      if (options.recolour) options.recolour();
    });
    return button;
  });
  var legend = el('div', 'blp-legend', colours), what = el('div', 'blp-what', colours);

  function showColour() {
    var rides = colour === 'rides';
    var key = !rides ? 'cluster' : dark ? 'dark' : 'rides';
    if (key === shown) return;
    shown = key;
    buttons.forEach(function (b) {
      b.setAttribute('aria-pressed', String(b.getAttribute('data-colour') === colour));
    });
    colourName.textContent = rides ? 'Häufigkeit' : 'Cluster';
    legend.textContent = '';
    LEGEND[key].forEach(function (step) {
      var item = el('span', '', legend), swatch = el('i', '', item);
      swatch.style.background = step[1];
      if (step[2]) swatch.style.border = '2px solid ' + step[2];
      item.appendChild(document.createTextNode(step[0]));
    });
    what.textContent = rides ? 'Fahrten durch die Kachel' : 'Cluster: alle vier Nachbarkacheln besucht';
  }
  function redrawTiles() {
    map.eachLayer(function (layer) { if (coloured(layer)) layer.redraw(); });
  }
  var drawnDark = null, shown = '';
  function bar() {
    var a = +start.value, b = +end.value;
    fill.style.left = 100 * a / months + '%';
    fill.style.width = 100 * Math.max(0, b - a) / months + '%';
    // both thumbs on one spot: the one with room to move is on top
    start.style.zIndex = a >= b && a > months / 2 ? 2 : 1;
    end.style.zIndex = a >= b && a > months / 2 ? 1 : 2;
  }
  function show() {
    start.value = range.from ? monthOf(range.from) : 0;
    end.value = range.to ? monthOf(range.to) : months;
    from.value = range.from;
    to.value = range.to;
    name.textContent = title(range);
    box.classList.toggle('blp-on', !!(range.from || range.to));
    var at = 'own';
    list.forEach(function (p, i) { if (at === 'own' && p[1] === range.from && p[2] === range.to) at = '' + i; });
    pick.value = at;
    bar();
  }
  function set(next) {
    var changed = next.from !== range.from || next.to !== range.to;
    range = next;
    show();
    if (!changed) return;
    store();
    map.eachLayer(function (layer) { if (ours(layer)) layer.redraw(); });
    if (options.change) options.change();
  }
  // only the thumb moved changes: the other end keeps its exact day
  function slid(moved) {
    var a = +start.value, b = +end.value;
    if (moved === start) {
      if (a > b) start.value = a = b;
      return {from: a ? monthStart(a) : '', to: range.to};
    }
    if (b < a) end.value = b = a;
    return {from: range.from, to: b < months ? monthEnd(b) : ''};
  }
  [start, end].forEach(function (thumb) {
    thumb.addEventListener('input', function () { name.textContent = title(slid(thumb)); bar(); });
    thumb.addEventListener('change', function () {
      var next = slid(thumb);
      if (next.from && next.to && next.from > next.to) {
        if (thumb === start) next.to = monthEnd(+end.value); else next.from = monthStart(+start.value);
      }
      set(next);
    });
  });
  [from, to].forEach(function (input) {
    // a year being typed passes 0002, 0020, 0202: valid() lets none of them through
    function typed() {
      if (input.value && !valid(input.value)) return;
      var next = {from: from.value ? valid(from.value) : '', to: to.value ? valid(to.value) : ''};
      if (next.from && next.to && next.from > next.to) next = {from: next.to, to: next.from};
      set(next);
    }
    input.addEventListener('change', typed);
  });
  pick.addEventListener('change', function () {
    var p = list[+pick.value];
    if (p) set({from: p[1], to: p[2]});
  });
  head.addEventListener('click', function () {
    body.hidden = !body.hidden;
    head.setAttribute('aria-expanded', String(!body.hidden));
  });
  L.DomEvent.disableClickPropagation(box);
  L.DomEvent.disableScrollPropagation(box);
  // no map or planner shortcuts while typing a date or moving a thumb
  ['keydown', 'keyup', 'keypress', 'contextmenu', 'pointerdown', 'touchmove'].forEach(function (type) {
    box.addEventListener(type, function (e) { e.stopPropagation(); });
  });

  function visible() {                  // in the planner only with a BikeLog layer on
    var any = !!options.always, tiled = false, isDark = false;
    map.eachLayer(function (layer) {
      if (ours(layer)) any = true;
      if (coloured(layer)) tiled = true;
      if (darkBase(layer)) isDark = true;
    });
    box.hidden = !any;
    colours.hidden = colourLine.hidden = !tiled;
    var turned = drawnDark !== null && drawnDark !== isDark;
    dark = drawnDark = isDark;
    showColour();
    if (turned && colour === 'rides') redrawTiles();
  }
  show();
  var Control = L.Control.extend({options: {position: options.position || 'bottomright'}, onAdd: function () { return box; }});
  new Control().addTo(map);
  visible();
  var due = null;                       // a route being dragged adds and removes layers all the time
  map.on('layeradd layerremove', function () {
    if (due === null) due = setTimeout(function () { due = null; visible(); }, 0);
  });
}

var attached = false;
function attach(L, map, options) {
  if (attached) return;
  attached = true;
  patch(L);
  el('style', '', document.head).textContent = CSS;
  control(L, map, options || {});
}

window.BikeLogPeriod = {
  attach: attach,
  query: query,
  colour: function () { return colour; },
  link: function () {
    return 'bikelogfrom=' + range.from + '&bikelogto=' + range.to + '&bikelogcolour=' + colour;
  }
};

// BRouter-Web: loaded by config.js, before or after the app
if (window.BR) {
  var late = false;
  var hook = function () {
    if (!window.L || !window.L.TileLayer) return false;
    var map = window.BR.debug && window.BR.debug.map;
    if (!patched) {
      late = !!map;                     // the map has asked for tiles without the period already
      patch(window.L);
    }
    if (!map) return false;
    if (!attached) {
      attach(window.L, map);
      if (late) map.eachLayer(function (layer) { if (ours(layer) && tileQuery(layer)) layer.redraw(); });
    }
    return true;
  };
  document.addEventListener('DOMContentLoaded', hook);
  if (!hook()) {
    var tries = 0;
    var timer = setInterval(function () { if (hook() || ++tries > 1200) clearInterval(timer); }, 50);
  }
}
})();
"""


def step_names() -> list[str]:
    """1, 2, 3–4, ..., 25+"""
    steps = explorer.RIDE_STEPS
    return [str(low) if high - low == 1 else f"{low}–{high - 1}"
            for low, high in zip(steps, steps[1:])] + [f"{steps[-1]}+"]


def _seen(colour: tuple[int, int, int, int], base: tuple[int, int, int]) -> str:
    """An overlay colour as it looks on a base map."""
    share = colour[3] / 255
    return "#%02x%02x%02x" % tuple(round(b + (c - b) * share) for c, b in zip(colour, base))


def legends() -> dict[str, list[list[str]]]:
    """Per colouring: [label, colour, border] of its swatches, from the palettes the tiles have."""
    light, dark = (250, 250, 248), (38, 38, 38)
    v = explorer.VISITED_PALETTE
    return {"cluster": [[name, _seen(v[i], light)] for i, name in ((1, "besucht"), (2, "Cluster"),
                                                                   (3, "Max Cluster"))]
            + [["Max Square", _seen(v[0], light), _seen(v[5], light)]],
            "rides": [[name, _seen(c, light)] for name, c in zip(step_names(), explorer.RIDE_PALETTE[1:])],
            "dark": [[name, _seen(c, dark)] for name, c in zip(step_names(), explorer.RIDE_PALETTE_DARK[1:])]}


def script(first: datetime.date | None) -> str:
    """The script, its slider beginning at the month of first."""
    # a date from before GPS is a broken clock, not a ride
    month = f"{max(first, datetime.date(2000, 1, 1)):%Y-%m}" if first else None
    return _JS.replace("__FIRST__", json.dumps(month)).replace("__LEGEND__", json.dumps(legends()))
