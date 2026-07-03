// Vanilla JS: minimal DOM helper
const $ = (s, p) => (p || document).querySelector(s);
const $$ = (s, p) => Array.from((p || document).querySelectorAll(s));
const on = (el, ev, fn) => (typeof el === 'string' ? $(el) : el).addEventListener(ev, fn);
const delegate = (parent, selector, ev, fn) => {
    $(parent).addEventListener(ev, (e) => {
        const t = e.target.closest(selector);
        if (t) fn(e, t);
    });
};
const html = (el, h) => { if (typeof el === 'string') el = $(el); el.innerHTML = h; return el; };
const append = (el, child) => { (typeof el === 'string' ? $(el) : el).appendChild(child); return el; };
const remove = (el) => { (typeof el === 'string' ? $(el) : el).remove(); };
