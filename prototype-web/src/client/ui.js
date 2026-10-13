// Небольшие DOM-хелперы: элементы, тосты, модальные окна, диалоги.
import { SPEAKERS } from '../core/campaign.js';
import { RES } from '../core/config.js';

export function el(tag, attrs = {}, ...children) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k === 'class') e.className = v;
    else if (k === 'style' && typeof v === 'object') Object.assign(e.style, v);
    else if (k.startsWith('on')) e.addEventListener(k.slice(2).toLowerCase(), v);
    else if (k === 'html') e.innerHTML = v;
    else if (v === true) e.setAttribute(k, '');
    else e.setAttribute(k, v);
  }
  for (const c of children.flat()) {
    if (c == null || c === false) continue;
    e.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return e;
}

export const $ = (id) => document.getElementById(id);

export function clear(node) { while (node.firstChild) node.removeChild(node.firstChild); return node; }

export function fmt(n) {
  n = Math.floor(n);
  if (n >= 1e6) return (n / 1e6).toFixed(n >= 1e7 ? 0 : 1) + 'M';
  if (n >= 1e4) return (n / 1e3).toFixed(n >= 1e5 ? 0 : 1) + 'k';
  return String(n);
}

export function fmtTime(sec) {
  sec = Math.max(0, Math.ceil(sec));
  if (sec < 60) return `${sec}с`;
  const m = Math.floor(sec / 60), s = sec % 60;
  if (m < 60) return s ? `${m}м ${s}с` : `${m}м`;
  const h = Math.floor(m / 60);
  return `${h}ч ${m % 60}м`;
}

export function costLabel(res, amount) {
  return el('span', { style: { color: RES[res].color } }, `${RES[res].icon} ${fmt(amount)}`);
}

export function starsEl(n, max = 3) {
  const s = el('span', { class: 'stars' });
  for (let i = 0; i < max; i++) s.append(el('span', { class: i < n ? '' : 'off' }, '★'));
  return s;
}

export function toast(msg, kind = '') {
  const t = el('div', { class: `toast ${kind}` }, msg);
  $('toasts').append(t);
  setTimeout(() => t.remove(), 2500);
}

const modalStack = [];
export function modal({ title, body, buttons = [], onClose, closable = true, width }) {
  const root = $('modal-root');
  const back = el('div', { class: 'modal-back' });
  const box = el('div', { class: 'modal', style: width ? { width: `min(${width}px, 100%)` } : null });
  const close = () => {
    back.remove();
    const i = modalStack.indexOf(api);
    if (i >= 0) modalStack.splice(i, 1);
    onClose?.();
  };
  const head = el('header', {}, el('h2', {}, title), closable ? el('button', { onclick: close }, '✕') : null);
  const bodyEl = el('div', { class: 'body' });
  const api = { close, body: bodyEl, root: back };
  if (typeof body === 'function') body(bodyEl, api); else if (body) bodyEl.append(body);
  box.append(head, bodyEl);
  if (buttons.length) {
    const foot = el('div', { class: 'foot' });
    for (const b of buttons) foot.append(el('button', { class: b.cls || '', onclick: () => b.onClick?.(api) }, b.text));
    box.append(foot);
  }
  back.append(box);
  back.addEventListener('pointerdown', (e) => { if (e.target === back && closable) close(); });
  root.append(back);
  modalStack.push(api);
  return api;
}

export function anyModalOpen() { return modalStack.length > 0 || !!document.querySelector('.dialogue, .title-screen'); }

export function confirmBox(title, text, okText, onOk) {
  modal({
    title, body: el('div', {}, text),
    buttons: [
      { text: 'Отмена', onClick: (m) => m.close() },
      { text: okText, cls: 'gold', onClick: (m) => { m.close(); onOk(); } },
    ],
  });
}

// Сюжетный диалог: lines = [[speakerId, text], ...]
export function dialogue(lines, onDone) {
  const root = $('modal-root');
  const wrap = el('div', { class: 'dialogue' });
  let i = 0, typing = null, shown = 0;
  const portrait = el('div', { class: 'portrait' });
  const who = el('div', { class: 'who' });
  const text = el('div', { class: 'text' });
  const box = el('div', { class: 'box' }, portrait, el('div', { style: { flex: 1 } }, who, text, el('div', { class: 'hint' }, 'нажми, чтобы продолжить ▸')));
  const finish = () => { clearInterval(typing); wrap.remove(); onDone?.(); };
  const skip = el('button', { class: 'skip', onclick: (e) => { e.stopPropagation(); finish(); } }, 'Пропустить ⏭');
  wrap.append(skip, box);
  const show = () => {
    const [sid, line] = lines[i];
    const sp = SPEAKERS[sid] || SPEAKERS.narrator;
    portrait.style.display = sp.name ? '' : 'none';
    portrait.style.background = sp.color;
    portrait.textContent = sp.name ? sp.name[0] : '';
    who.textContent = sp.name;
    who.style.color = sp.color;
    text.style.fontStyle = sp.name ? 'normal' : 'italic';
    shown = 0; text.textContent = '';
    clearInterval(typing);
    typing = setInterval(() => {
      shown += 2;
      text.textContent = line.slice(0, shown);
      if (shown >= line.length) clearInterval(typing);
    }, 16);
  };
  box.addEventListener('click', () => {
    const line = lines[i][1];
    if (shown < line.length) { shown = line.length; text.textContent = line; clearInterval(typing); return; }
    i++;
    if (i >= lines.length) finish(); else show();
  });
  root.append(wrap);
  show();
}
