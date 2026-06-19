#!/usr/bin/env node
const fs = require('fs');
const path = require('path');
const blessed = require('blessed');

const VAULT_DIR = process.env.HOME + '/.vault';
const DATA_DIR = VAULT_DIR + '/data';
const VERSIONS_DIR = DATA_DIR + '/versions';
const OBJECTS_DIR = DATA_DIR + '/objects';

function readFile(p) {
  try { return fs.readFileSync(p, 'utf8'); } catch { return null; }
}

function readNames() {
  const raw = readFile(DATA_DIR + '/names.json');
  if (!raw) return {};
  try { return JSON.parse(raw); } catch { return {}; }
}

function readHead() {
  return (readFile(VAULT_DIR + '/HEAD') || 'genesis').trim();
}

function listVersions() {
  try {
    return fs.readdirSync(VERSIONS_DIR)
      .filter(f => f.endsWith('.json'))
      .map(f => f.replace('.json', ''));
  } catch { return []; }
}

function loadVersion(sic) {
  const raw = readFile(VERSIONS_DIR + '/' + sic + '.json');
  if (!raw) return null;
  const v = {};
  for (const line of raw.split('\n')) {
    const i = line.indexOf('=');
    if (i < 0) continue;
    const k = line.slice(0, i);
    const val = line.slice(i + 1);
    if (k === 'changed_files') v.changed_files = val ? val.split(',') : [];
    else if (k === 'timestamp') v.timestamp = parseInt(val);
    else v[k] = val;
  }
  return v;
}

function loadTree(treeHash) {
  const p = OBJECTS_DIR + '/' + treeHash.slice(0, 2) + '/' + treeHash.slice(2);
  const raw = readFile(p);
  if (!raw) return [];
  return raw.split('\n').filter(Boolean).map(line => {
    const parts = line.split('\0');
    return { name: parts[0] || '', blob_hash: parts[1] || '', permissions: parseInt(parts[2]) || 0 };
  });
}

function buildFileTree(entries) {
  const root = { name: '.', children: {}, files: [] };
  for (const e of entries) {
    const parts = e.name.split('/');
    let node = root;
    for (let i = 0; i < parts.length - 1; i++) {
      if (!node.children[parts[i]]) node.children[parts[i]] = { name: parts[i], children: {}, files: [] };
      node = node.children[parts[i]];
    }
    node.files.push(parts[parts.length - 1]);
  }
  return root;
}

function renderTreeLines(node, prefix, isLast, lines) {
  const base = ' ' + prefix + (isLast ? '└── ' : '├── ');
  if (node.name !== '.') {
    lines.push({ text: base + node.name + '/', isDir: true, depth: prefix.length / 2 });
  }
  const items = [];
  const dirNames = Object.keys(node.children).sort();
  for (const d of dirNames) items.push({ type: 'dir', name: d });
  for (const f of node.files.sort()) items.push({ type: 'file', name: f });
  const childPrefix = node.name === '.' ? prefix : prefix + (isLast ? '    ' : '│   ');
  items.forEach((item, i) => {
    const last = i === items.length - 1;
    if (item.type === 'dir') {
      renderTreeLines(node.children[item.name], childPrefix, last, lines);
    } else {
      lines.push({ text: ' ' + childPrefix + (last ? '└── ' : '├── ') + item.name, isDir: false, depth: childPrefix.length / 2 });
    }
  });
}

function formatTime(ts) {
  const d = new Date(ts * 1000);
  return d.toISOString().replace('T', ' ').slice(0, 19);
}

function short(sic) {
  return sic ? sic.slice(0, 6) : '??????';
}

const screen = blessed.screen({
  smartCSR: true,
  title: 'VAULT TUI',
  dockBorders: true,
  fullUnicode: true,
});

const borderCh = { type: 'line', fg: 'white' };

const mainBox = blessed.box({
  parent: screen,
  top: 0,
  left: 0,
  width: '100%',
  height: '100%',
  style: { bg: 'black', fg: 'white' },
});

const headerBar = blessed.box({
  parent: mainBox,
  top: 0,
  left: 0,
  width: '100%',
  height: 3,
  style: { bg: 'black', fg: 'white' },
  border: { type: 'line', fg: 'white', bottom: true },
  content: '',
  tags: true,
});

const leftPanel = blessed.box({
  parent: mainBox,
  top: 3,
  left: 0,
  width: '40%',
  height: '100%-6',
  style: { bg: 'black', fg: 'white' },
  border: { type: 'line', fg: 'white', right: true },
  label: ' Versions ',
});

const rightPanel = blessed.box({
  parent: mainBox,
  top: 3,
  left: '40%',
  width: '60%',
  height: '100%-6',
  style: { bg: 'black', fg: 'white' },
  border: { type: 'line', fg: 'white' },
  label: ' Details ',
});

const footerBar = blessed.box({
  parent: mainBox,
  bottom: 0,
  left: 0,
  width: '100%',
  height: 3,
  style: { bg: 'black', fg: 'white' },
  border: { type: 'line', fg: 'white', top: true },
  content: '',
  tags: true,
});

const versionList = blessed.list({
  parent: leftPanel,
  top: 1,
  left: 1,
  width: '100%-2',
  height: '100%-2',
  style: {
    bg: 'black',
    fg: 'white',
    selected: { bg: 'white', fg: 'black' },
    item: { bg: 'black', fg: 'white' },
  },
  keys: true,
  vi: true,
  items: [],
});

const detailContent = blessed.box({
  parent: rightPanel,
  top: 1,
  left: 1,
  width: '100%-2',
  height: '100%-2',
  style: { bg: 'black', fg: 'white' },
  scrollable: true,
  alwaysScroll: true,
  scrollbar: { ch: '░', fg: 'white', bg: 'black' },
});

let allVersions = [];
let versionMap = {};
let selectedIdx = 0;

function loadData() {
  const sics = listVersions();
  const names = readNames();
  const head = readHead();
  const nameToSic = {};
  for (const [k, v] of Object.entries(names)) {
    if (k !== 'HEAD') nameToSic[v] = k;
  }

  allVersions = [];
  versionMap = {};
  for (const sic of sics) {
    const v = loadVersion(sic);
    if (v) {
      v.sic = sic;
      v.short_sic = short(sic);
      v.resolvedName = nameToSic[sic] || v.name || short(sic);
      v.isHead = (sic === head);
      allVersions.push(v);
      versionMap[sic] = v;
    }
  }
  allVersions.sort((a, b) => (a.timestamp || 0) - (b.timestamp || 0));
  selectedIdx = Math.max(0, allVersions.findIndex(v => v.isHead));
  if (selectedIdx < 0) selectedIdx = 0;
}

function renderVersionList() {
  const items = allVersions.map((v, i) => {
    let prefix = '  ';
    if (v.isHead) prefix = '● ';
    else if (i === 0) prefix = '○ ';
    else prefix = '  ';
    const label = (v.resolvedName || short(v.sic));
    return prefix + label + (v.isHead ? '  ← HEAD' : '');
  });
  versionList.setItems(items);
  versionList.select(selectedIdx);
  screen.render();
}

function renderDetail(idx) {
  const v = allVersions[idx];
  if (!v) {
    detailContent.setContent(' No version selected.');
    screen.render();
    return;
  }

  let treeLines = [];
  let fileCount = 0;
  if (v.tree_hash) {
    const entries = loadTree(v.tree_hash);
    fileCount = entries.length;
    if (entries.length > 0) {
      const tree = buildFileTree(entries);
      renderTreeLines(tree, '', true, treeLines);
    }
  }

  const changed = (v.changed_files || []).filter(Boolean);
  const lines = [];
  lines.push(' Name:      ' + (v.name || '(unnamed)'));
  lines.push(' SIC:       ' + v.sic);
  lines.push(' Short:     ' + v.short_sic);
  lines.push(' Parent:    ' + (v.parent_sic ? short(v.parent_sic) + '  (' + v.parent_sic + ')' : '(none)'));
  lines.push(' Tree:      ' + (v.tree_hash ? short(v.tree_hash) + '  (' + v.tree_hash + ')' : '(none)'));
  lines.push(' Author:    ' + (v.author_id || 'unknown'));
  lines.push(' Timestamp: ' + (v.timestamp ? formatTime(v.timestamp) : 'unknown'));
  lines.push(' Message:   ' + (v.message || ''));
  lines.push('');
  lines.push('── Files (' + fileCount + ') ──');
  for (const f of changed) {
    lines.push('  ' + f);
  }
  if (treeLines.length > 0) {
    lines.push('');
    lines.push('── Tree ──');
    for (const tl of treeLines) {
      lines.push(tl.text);
    }
  }
  if (v.tree_hash) {
    lines.push('');
    lines.push('── Objects ──');
    const entries = loadTree(v.tree_hash);
    for (const e of entries) {
      lines.push(' ' + e.name + '  [' + short(e.blob_hash) + ']  (' + e.permissions.toString(8) + ')');
    }
  }
  detailContent.setContent('\n' + lines.join('\n'));
  detailContent.setScrollPerc(0);
  screen.render();
}

function updateFooter() {
  const headVer = allVersions.find(v => v.isHead);
  const headName = headVer ? headVer.resolvedName || headVer.name || short(headVer.sic) : 'none';
  footerBar.setContent(' {white-fg}↑↓{/white-fg} Navigate  {white-fg}q{/white-fg} Quit  {white-fg}r{/white-fg} Refresh  |  Versions: ' + allVersions.length + '  |  HEAD: ' + headName + '  |  ' + new Date().toLocaleTimeString());
  screen.render();
}

function updateHeader() {
  headerBar.setContent(' {white-fg}VAULT TUI{/white-fg}  —  Cryptographic Version Control');
  screen.render();
}

function refresh() {
  loadData();
  renderVersionList();
  if (allVersions.length > 0) renderDetail(selectedIdx);
  updateFooter();
  updateHeader();
}

versionList.on('select', (item, idx) => {
  selectedIdx = idx;
  renderDetail(idx);
});

versionList.key('up', () => {
  if (selectedIdx > 0) selectedIdx--;
  versionList.select(selectedIdx);
  renderDetail(selectedIdx);
  screen.render();
});

versionList.key('down', () => {
  if (selectedIdx < allVersions.length - 1) selectedIdx++;
  versionList.select(selectedIdx);
  renderDetail(selectedIdx);
  screen.render();
});

screen.key(['q', 'C-c'], () => process.exit(0));
screen.key('r', () => refresh());

screen.key('tab', () => {
  if (versionList.focused) {
    detailContent.focus();
  } else {
    versionList.focus();
  }
  screen.render();
});

refresh();
versionList.focus();
screen.render();
