const API = '/api';

async function api(path, opts) {
  const r = await fetch(API + path, {
    headers: { 'Content-Type': 'application/json', ...(opts || {}).headers },
    ...opts
  });
  const t = await r.text();
  try { return JSON.parse(t); } catch { return t; }
}

function $(id) { return document.getElementById(id); }

function msg(el, text, type) {
  const e = typeof el === 'string' ? $(el) : el;
  if (!e) return;
  e.textContent = text;
  e.className = 'msg ' + (type || 'info');
}

function fmtBytes(b) {
  if (!b) return '0 B';
  if (b < 1024) return b + ' B';
  if (b < 1048576) return (b / 1024).toFixed(1) + ' KB';
  return (b / 1048576).toFixed(1) + ' MB';
}

function fmtUptime(s) {
  if (!s) return '--';
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  let r = '';
  if (d) r += d + 'd ';
  if (h) r += h + 'h ';
  r += m + 'm';
  return r;
}

function backLink() {
  return '<a href="/" class="btn secondary" style="display:inline-block;margin-bottom:1rem">&larr; Voltar</a>';
}
