let ws = null;
let state = null;
let myId = -1;
let powerMode = null;   // 'PEEK_OWN' | 'PEEK_OPPONENT' | 'SWAP' | 'LOOK_SWAP' | null
let selected = [];      // [{playerId, position}] for two-card powers
let toastTimer = null;

// Peeked / just-placed cards are only shown for a few seconds, then turn face-down again
// (you have to remember them). The server still tracks what each player has seen.
const REVEAL_MS = 3500;
const revealUntil = new Map();   // key -> timestamp
const lastSig = new Map();       // key -> signature of the known card last seen at that slot

function sigOf(c) { return `${c.label}|${c.value}|${c.color}`; }
function trackReveals(st) {
  const now = Date.now();
  const cur = new Map();   // key -> signature of the known card now in that slot
  st.players.forEach(p => {
    p.cards.forEach(c => { if (c.occupied && c.known) cur.set(`${p.playerId}:${c.position}`, sigOf(c)); });
    // Extra cards (flip penalties, gifts) always stay face-down until the game ends.
  });

  // A swap moves already-known cards to a new slot. That is not new information, so it must
  // never flash the card face-up again (it would expose the swapped card to whoever knew it).
  // Cards that left a slot are matched to the same card appearing in another slot and simply
  // go face-down immediately (you could see them up to the moment of the swap, which is enough).
  const moved = new Map();   // sig -> [remaining reveal-until timestamps]
  lastSig.forEach((sig, key) => {
    if (cur.get(key) !== sig) {
      if (!moved.has(sig)) moved.set(sig, []);
      moved.get(sig).push(revealUntil.get(key) || 0);
    }
  });

  const nextSig = new Map();
  const nextReveal = new Map();
  cur.forEach((sig, key) => {
    nextSig.set(key, sig);
    if (lastSig.get(key) === sig) {                 // unchanged slot: keep its timer
      nextReveal.set(key, revealUntil.get(key) || 0);
      return;
    }
    const carried = moved.get(sig);
    if (carried && carried.length) {                // card moved here from another slot
      carried.shift();
      nextReveal.set(key, 0);                       // after a swap it goes face-down at once
    } else {                                        // genuinely new knowledge (peek, replace, ...)
      nextReveal.set(key, now + REVEAL_MS);
      setTimeout(() => render(), REVEAL_MS + 60);
    }
  });
  lastSig.clear();       nextSig.forEach((v, k) => lastSig.set(k, v));
  revealUntil.clear();   nextReveal.forEach((v, k) => revealUntil.set(k, v));
}
// A flip is public: everyone sees the flipped card for a moment (and only a moment).
const FLIP_MS = 2200;
let flipReveal = null;   // { playerId, position, card, success, until }
let lastFlipAttempt = 0;
function showFlip(f) {
  flipReveal = {
    playerId: f.targetPlayer, position: f.position, success: f.success,
    card: { occupied: true, known: true, label: f.label, value: f.value, color: f.color },
    until: Date.now() + FLIP_MS
  };
  setTimeout(() => render(), FLIP_MS + 60);
}
function activeFlip(playerId, pos) {
  return flipReveal && flipReveal.until > Date.now()
    && flipReveal.playerId === playerId && flipReveal.position === pos ? flipReveal : null;
}
function isLookCard(key) {
  const [a, xa, b, xb] = state.pendingLookCards || [];
  return key === `${a}:${xa}` || key === `${b}:${xb}`;
}
function faceUp(key) {
  if (!state) return false;
  if (state.phase === 'MEMORIZATION' || state.phase === 'FINISHED') return true;
  if (state.pendingLook && isLookCard(key)) return true;   // King: keep both visible while deciding
  return (revealUntil.get(key) || 0) > Date.now();
}

const $ = id => document.getElementById(id);

/* ---------- what the other players are doing ---------- */
// The server sends the public activity of each action (draw, replace, discard, peek, swap).
// Hidden identities are never part of it, only WHICH cards were involved, so we pulse those
// slots and show a short line like "Bob swapped his own card with your card".
const ACTIVITY_MS = 6000, HL_MS = 3500;
let activityText = '', activityUntil = 0;
const highlights = new Map();   // "player:pos" -> { until, kind: 'swap'|'replace'|'peek' }
let pendingAnims = [];          // visual moves to play after the next render

function slotName(p, actor) {
  if (p === myId) return 'your';
  if (p === actor) return 'their own';
  return `${nameOf(p)}'s`;
}
function describeActivity(list) {
  const parts = [];
  for (let i = 0; i < list.length; i++) {
    const a = list[i];
    const who = nameOf(a.actor);
    if (a.kind === 'DRAW') {
      parts.push(a.fromDiscard ? `${who} took the ${a.label} from the discard pile` : `${who} drew from the deck`);
    } else if (a.kind === 'REPLACE') {
      parts.push(`${who} replaced one of their cards (discarded ${a.label})`);
    } else if (a.kind === 'DISCARD') {
      parts.push(`${who} discarded ${a.label}`);
    } else if (a.kind === 'PEEK') {
      const next = list[i + 1];
      if (next && next.kind === 'PEEK' && next.actor === a.actor) {   // King: two cards at once
        parts.push(`${who} looked at ${slotName(a.p1, a.actor)} card and ${slotName(next.p1, a.actor)} card`);
        i++;
      } else {
        parts.push(`${who} looked at ${slotName(a.p1, a.actor)} card`);
      }
    } else if (a.kind === 'SWAP') {
      parts.push(`${who} swapped ${slotName(a.p1, a.actor)} card with ${slotName(a.p2, a.actor)} card`);
    } else if (a.kind === 'CAMBIO') {
      parts.push(`${who} called Cambio`);
    }
  }
  return parts.join(' · ');
}
function trackActivity(list) {
  const all = list || [];
  if (!all.length) return;
  // Text banner: only what OTHER players did (your own moves already give you feedback).
  const others = all.filter(a => a.actor !== myId);
  if (others.length) {
    activityText = describeActivity(others);
    activityUntil = Date.now() + ACTIVITY_MS;
    setTimeout(() => render(), ACTIVITY_MS + 60);
  }
  // Visual marks + motion for every swap / replace, and for other players' peeks.
  const until = Date.now() + HL_MS;
  all.forEach(a => {
    if (a.kind === 'PEEK' && a.actor !== myId) highlights.set(`${a.p1}:${a.x1}`, { until, kind: 'peek' });
    if (a.kind === 'REPLACE') {
      highlights.set(`${a.p1}:${a.x1}`, { until, kind: 'replace' });
      pendingAnims.push({ type: 'replace', key: `${a.p1}:${a.x1}` });
    }
    if (a.kind === 'SWAP') {
      const k1 = `${a.p1}:${a.x1}`, k2 = `${a.p2}:${a.x2}`;
      highlights.set(k1, { until, kind: 'swap' }); highlights.set(k2, { until, kind: 'swap' });
      pendingAnims.push({ type: 'swap', a: k1, b: k2 });
    }
  });
  setTimeout(() => render(), HL_MS + 60);
}
// Cards visibly fly: a swap crosses the two cards over, a replace drops the new card in from the deck.
function slotEl(key) { return document.querySelector(`.pcard[data-key="${key}"]`); }
function playAnimations() {
  const list = pendingAnims; pendingAnims = [];
  const fly = (el, from) => {
    if (!el || !from) return;
    const to = el.getBoundingClientRect();
    const dx = from.left - to.left, dy = from.top - to.top;
    el.style.zIndex = 5;
    el.animate(
      [{ transform: `translate(${dx}px, ${dy}px) scale(1.15)`, opacity: .9 }, { transform: 'none', opacity: 1 }],
      { duration: 900, easing: 'cubic-bezier(.2,.8,.2,1)' }
    ).onfinish = () => { el.style.zIndex = ''; };
  };
  list.forEach(an => {
    if (an.type === 'swap') {
      const ea = slotEl(an.a), eb = slotEl(an.b);
      if (!ea || !eb) return;
      const ra = ea.getBoundingClientRect(), rb = eb.getBoundingClientRect();
      fly(ea, rb); fly(eb, ra);               // each card travels from the other one's spot
    } else if (an.type === 'replace') {
      const deck = $('deck');
      if (deck) fly(slotEl(an.key), deck.getBoundingClientRect());
    }
  });
}
function highlightOf(key) { const h = highlights.get(key); return h && h.until > Date.now() ? h : null; }

const POWER_TEXT = {
  PEEK_OWN: 'peek at one of your own cards',
  PEEK_OPPONENT: "peek at someone else's card",
  BLIND_SWAP: 'swap any two cards without looking',
  LOOK_AND_SWAP: 'look at two cards and decide whether to swap'
};

/* ---------- helpers ---------- */
function send(obj) { if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj)); }
// Short confirmation shown after your own move once the turn has passed ("You looked at a card").
let flashMsg = '', flashUntil = 0;
const FLASH = {
  PEEK_OWN_CARD: () => 'You looked at a card',
  PEEK_OPPONENT_CARD: () => 'You looked at a card',
  REPLACE_CARD_WITH_DRAWN: () => 'You replaced a card',
  DISCARD_DRAWN_CARD: () => 'You discarded a card',
  BLIND_SWAP: () => 'You swapped two cards',
  DECIDE_SWAP: x => (x.shouldSwap ? 'You swapped the two cards' : 'You left the cards where they were')
};
function action(name, extra = {}) {
  if (FLASH[name]) {
    flashMsg = FLASH[name](extra); flashUntil = Date.now() + 3500;
    setTimeout(() => render(), 3560);
  }
  send({ type: 'action', action: name, ...extra });
}
function playerById(id) { return state?.players?.find(p => p.playerId === id); }
function me() { return playerById(myId); }
function nameOf(id) { return playerById(id)?.name ?? `Player ${id}`; }
function isMyTurn() { return !!state && state.currentPlayer === myId; }
function inPlay() { return state && (state.phase === 'PLAYING' || state.phase === 'FINAL_ROUND'); }
function myStage() { return state?.turnStage; }
function esc(s) { return String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c])); }
function toast(msg) {
  const t = $('toast');
  t.textContent = msg; t.classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.add('hidden'), 2600);
}
function resetPower() { powerMode = null; selected = []; }

/* ---------- card rendering ---------- */
function cardEl(card, { selectable = false, flippable = false, picked = false, onClick = null } = {}) {
  const d = document.createElement('div');
  d.className = 'pcard';
  if (!card || !card.occupied) { d.classList.add('empty'); return d; }
  if (card.known) {
    d.classList.add('up');
    if (card.color === 'RED') d.classList.add('red');
    d.innerHTML = `<span class="lbl">${esc(card.label)}</span><span class="pts">${esc(card.value)}</span>`;
  } else {
    d.classList.add('down');
  }
  if (selectable) d.classList.add('selectable');
  if (flippable) d.classList.add('flippable');
  if (picked) d.classList.add('picked');
  if ((selectable || flippable) && onClick) d.onclick = onClick;
  return d;
}

function faceCardEl(label, value, color) {
  return cardEl({ occupied: true, known: true, label, value, color });
}

/* ---------- what can be clicked right now ---------- */
function canFlip(targetId) {
  if (!inPlay() || !state.topDiscard) return false;
  if (state.cambioCaller === myId) return false;          // caller can't flip
  if (state.cambioCaller === targetId) return false;      // caller's cards are protected
  if (state.topDiscardOwner === myId) return false;       // not on your own discard
  if (state.pendingFlipGift) return false;
  if (myStage() === 'AFTER_DRAW' || myStage() === 'LOOK_DECISION') return false;  // finish your turn first
  return true;
}

function protectedCaller(id) { return state.phase === 'FINAL_ROUND' && state.cambioCaller === id; }

// Returns 'select' (pink glow, an action), 'flip' (plain clickable) or null.
function cardMode(playerId, card) {
  if (!card.occupied) return null;
  if (state.phase === 'MEMORIZATION' || state.phase === 'FINISHED') return null;

  if (state.pendingFlipGift) return playerId === myId ? 'select' : null;
  if (myStage() === 'LOOK_DECISION') return null;

  if (myStage() === 'AFTER_DRAW' && isMyTurn()) {
    if (powerMode === 'PEEK_OWN') return playerId === myId ? 'select' : null;
    if (powerMode === 'PEEK_OPPONENT') return playerId !== myId && !protectedCaller(playerId) ? 'select' : null;
    if (powerMode === 'SWAP' || powerMode === 'LOOK_SWAP') return protectedCaller(playerId) ? null : 'select';
    return playerId === myId ? 'select' : null;           // default: replace one of your cards
  }
  return canFlip(playerId) ? 'flip' : null;
}

function handleCardClick(playerId, card) {
  const position = card.position;
  const mode = cardMode(playerId, card);
  if (!mode) return;

  if (state.pendingFlipGift) {
    action('GIVE_CARD_AFTER_FLIP', { position, extraCardIndex: -1 });
    return;
  }
  if (myStage() === 'AFTER_DRAW' && isMyTurn()) {
    if (powerMode === 'PEEK_OWN') { action('PEEK_OWN_CARD', { position }); resetPower(); return; }
    if (powerMode === 'PEEK_OPPONENT') {
      action('PEEK_OPPONENT_CARD', { targetPlayer: playerId, targetPosition: position }); resetPower(); return;
    }
    if (powerMode === 'SWAP' || powerMode === 'LOOK_SWAP') {
      const i = selected.findIndex(s => s.playerId === playerId && s.position === position);
      if (i >= 0) selected.splice(i, 1);
      else if (selected.length < 2) selected.push({ playerId, position });
      if (powerMode === 'LOOK_SWAP' && selected.length === 2) { submitTwoCardPower(false); return; }
      render();
      return;
    }
    action('REPLACE_CARD_WITH_DRAWN', { position });
    return;
  }
  // plain flip
  lastFlipAttempt = Date.now();
  action('FLIP_CARD', {
    position: playerId === myId ? position : -1,
    targetPlayer: playerId === myId ? -1 : playerId,
    targetPosition: playerId === myId ? -1 : position
  });
}

function decideSwap(shouldSwap) { action('DECIDE_SWAP', { shouldSwap }); }

function giveExtra(index) { action('GIVE_CARD_AFTER_FLIP', { position: -1, extraCardIndex: index }); }

function submitTwoCardPower(shouldSwap) {
  if (selected.length !== 2) return;
  // Blind swap acts at once; King first LOOKS (step 1), the swap decision is a separate step.
  action(powerMode === 'SWAP' ? 'BLIND_SWAP' : 'LOOK_TWO_CARDS', {
    targetPlayer: selected[0].playerId, targetPosition: selected[0].position,
    secondTargetPlayer: selected[1].playerId, secondTargetPosition: selected[1].position,
    shouldSwap
  });
  resetPower();
}

function startPower() {
  const p = state.pendingDrawPower;
  selected = [];
  powerMode = p === 'PEEK_OWN' ? 'PEEK_OWN'
    : p === 'PEEK_OPPONENT' ? 'PEEK_OPPONENT'
    : p === 'BLIND_SWAP' ? 'SWAP'
    : p === 'LOOK_AND_SWAP' ? 'LOOK_SWAP' : null;
  render();
}

/* ---------- hands ---------- */
// Grid positions are laid out as two rows: [2,3] on top and [0,1] below, so the two
// cards you memorized (0 and 1) sit on the bottom row, nearest your name.
const ORDER = [2, 3, 0, 1];

function renderHand(p, { opponent = false } = {}) {
  const wrap = document.createElement('div');
  wrap.className = 'hand-wrap';
  const hand = document.createElement('div');
  hand.className = 'hand';
  const finished = state.phase === 'FINISHED';

  ORDER.forEach(pos => {
    const c = p.cards.find(x => x.position === pos);
    const mode = cardMode(p.playerId, c);
    const fl = activeFlip(p.playerId, pos);
    const shown = fl ? fl.card
      : c.occupied && c.known && faceUp(`${p.playerId}:${pos}`) ? c : { ...c, known: false };
    const picked = selected.some(s => s.playerId === p.playerId && s.position === pos)
      || (state.pendingLook && isLookCard(`${p.playerId}:${pos}`) && myStage() === 'LOOK_DECISION');
    const el = cardEl(shown, {
      selectable: mode === 'select', flippable: mode === 'flip', picked,
      onClick: () => handleCardClick(p.playerId, c)
    });
    if (fl) el.classList.add(fl.success ? 'flip-ok' : 'flip-bad');
    el.dataset.key = `${p.playerId}:${pos}`;
    const hl = highlightOf(`${p.playerId}:${pos}`);
    if (hl) {
      el.classList.add('hl', `hl-${hl.kind}`);
      const t = document.createElement('span');
      t.className = 'tag';
      t.textContent = hl.kind === 'swap' ? '⇄' : hl.kind === 'replace' ? '↻' : '👁';
      el.appendChild(t);
    }
    hand.appendChild(el);
  });

  // Extra cards stay face-down (even your own) and are revealed when the game ends.
  if (p.extraCards && p.extraCards.length) {
    p.extraCards.forEach((val, i) => {
      const card = { occupied: true, known: finished, value: val, label: p.extraLabels?.[i] ?? val, color: p.extraColors?.[i] ?? 'NONE' };
      const giving = state.pendingFlipGift && p.playerId === myId;
      hand.appendChild(cardEl(card, { selectable: giving, onClick: () => giveExtra(i) }));
    });
  } else if (p.extraCardCount) {
    for (let i = 0; i < p.extraCardCount; i++) hand.appendChild(cardEl({ occupied: true, known: false }));
  }
  wrap.appendChild(hand);

  const bar = document.createElement('div');
  bar.className = 'namebar';
  if (p.playerId === state.currentPlayer && state.phase !== 'FINISHED') bar.classList.add('turn');
  if (finished && (state.winners || []).includes(p.playerId)) bar.classList.add('winner');
  let html = esc(p.name);
  if (finished) html += `<span class="score">${p.score} pts</span>`;
  else if (p.playerId === state.cambioCaller) html += '<span class="tag">CAMBIO</span>';
  bar.innerHTML = html;
  wrap.appendChild(bar);
  return wrap;
}

/* ---------- instruction text ---------- */
function instruction() {
  const s = state;
  const myTurn = isMyTurn();
  if (s.phase === 'MEMORIZATION') return 'Memorize your cards then press ready';
  if (s.phase === 'FINISHED') {
    const w = s.winners || [];
    const loser = s.players.find(p => p.cardCount > 6);
    if (loser) return `<b>${esc(loser.name)}</b> has more than 6 cards and loses.<span class="or">${w.length ? esc(w.map(nameOf).join(' & ')) + ' win' + (w.length === 1 ? 's' : '') : ''}</span>`;
    return w.length ? `<b>${esc(w.map(nameOf).join(' & '))}</b> ${w.length === 1 ? 'wins' : 'win'}!` : 'Game finished';
  }
  if (s.pendingFlipGift) return `Correct flip! Give <b>${esc(nameOf(s.pendingFlipGiftTarget))}</b> one of your cards`;
  if (isMyTurn() && myStage() === 'LOOK_DECISION') return 'Swap the two cards you looked at<span class="or">or</span>leave them where they are';
  if (isMyTurn() && myStage() === 'AFTER_DRAW') {
    if (powerMode === 'PEEK_OWN') return 'Tap one of your cards to peek at it';
    if (powerMode === 'PEEK_OPPONENT') return "Tap someone else's card to peek at it";
    if (powerMode === 'SWAP') return selected.length < 2 ? `Select two cards to swap (${selected.length}/2)` : 'Swap the selected cards?';
    if (powerMode === 'LOOK_SWAP') return `Choose two cards to look at (${selected.length}/2)`;
    if (s.pendingDrawFromDiscard) return 'Replace one of your cards';
    if (s.pendingDrawPower === 'NONE') return 'Discard<span class="or">or</span>Replace one of your cards';
    return `Discard to <b>${POWER_TEXT[s.pendingDrawPower]}</b><span class="or">or</span>Replace one of your cards`;
  }
  if (isMyTurn() && myStage() === 'DRAW') {
    return s.phase === 'FINAL_ROUND' ? 'Final turn: draw from the deck or discard pile' : 'Draw from the deck or discard pile';
  }
  if (!myTurn && flashMsg && Date.now() < flashUntil) return esc(flashMsg);
  if (s.currentDrew) {
    return `<b>${esc(nameOf(s.currentPlayer))}</b> ${s.currentDrewFromDiscard ? 'took the top discard' : 'drew from the deck'} and is deciding`;
  }
  return `<b>${esc(nameOf(s.currentPlayer))}</b> is choosing a card`;
}

function subText() {
  const s = state;
  if (s.phase === 'MEMORIZATION' || s.phase === 'FINISHED' || s.turnStage === 'LOOK_DECISION') return '';
  if (s.phase === 'FINAL_ROUND' && s.cambioCaller >= 0) {
    return `${nameOf(s.cambioCaller)} called Cambio. Everyone else gets one last turn.`;
  }
  return '';
}

/* ---------- main render ---------- */
function render() {
  if (!state) return;
  const s = state;
  const memo = s.phase === 'MEMORIZATION';
  const finished = s.phase === 'FINISHED';
  const myTurn = isMyTurn();
  const stage = myStage();
  const mine = me();

  $('instruction').innerHTML = instruction();
  $('sub').textContent = subText();
  const act = $('activity');
  const showAct = !!activityText && Date.now() < activityUntil && s.phase !== 'MEMORIZATION';
  act.classList.toggle('hidden', !showAct);
  if (showAct) act.textContent = activityText;

  // Ready button
  const readyBtn = $('ready');
  readyBtn.classList.toggle('hidden', !memo);
  if (memo) {
    const n = s.players.filter(p => p.ready).length;
    readyBtn.textContent = `Ready ${n} / ${s.players.length}`;
    readyBtn.disabled = !!mine?.ready;
  }

  // Table: deck, discard, drawn card
  const showTable = !memo;
  $('table').classList.toggle('hidden', !showTable);
  const deckSelectable = inPlay() && myTurn && stage === 'DRAW';
  $('deck').classList.toggle('selectable', deckSelectable);
  $('deck').disabled = !deckSelectable;

  const discard = $('discard');
  if (s.topDiscard) {
    discard.className = 'pile discard' + (s.topDiscardColor === 'RED' ? ' red' : '');
    discard.innerHTML = `<span class="lbl">${esc(s.topDiscardLabel || s.topDiscard)}</span><span class="pts">${esc(s.topDiscard)}</span>`;
  } else {
    discard.className = 'pile discard empty';
    discard.innerHTML = '';
  }
  const discardSelectable = deckSelectable && !!s.topDiscard;
  discard.classList.toggle('selectable', discardSelectable);
  discard.disabled = !discardSelectable;

  // Drawn card overlay + its buttons
  const hasDraw = !!s.pendingDraw && stage === 'AFTER_DRAW' && myTurn;
  $('drawn').classList.toggle('hidden', !hasDraw);
  if (hasDraw) {
    const dc = $('drawnCard'); dc.innerHTML = '';
    dc.appendChild(faceCardEl(s.pendingDrawLabel || s.pendingDraw, s.pendingDraw, s.pendingDrawColor));
    const btns = $('drawnButtons'); btns.innerHTML = '';
    const add = (text, cls, fn) => { const b = document.createElement('button'); b.className = 'btn ' + cls; b.textContent = text; b.onclick = fn; btns.appendChild(b); };
    if (powerMode) add('Cancel', 'secondary', () => { resetPower(); render(); });
    else if (!s.pendingDrawFromDiscard && s.pendingDrawPower !== 'NONE') add('Use Power', 'primary', startPower);
    else if (!s.pendingDrawFromDiscard) add('Discard', 'primary', () => action('DISCARD_DRAWN_CARD'));
  }

  // Two-card power confirmation buttons
  const pa = $('powerActions'); pa.innerHTML = '';
  const deciding = isMyTurn() && stage === 'LOOK_DECISION';
  const blindReady = powerMode === 'SWAP' && selected.length === 2;
  pa.classList.toggle('hidden', !(deciding || blindReady));
  const addBtn = (text, cls, fn) => { const b = document.createElement('button'); b.className = 'btn ' + cls; b.textContent = text; b.onclick = fn; pa.appendChild(b); };
  if (deciding) {
    addBtn('Swap', 'primary', () => decideSwap(true));
    addBtn('Keep', 'secondary', () => decideSwap(false));
  } else if (blindReady) {
    addBtn('Swap', 'primary', () => submitTwoCardPower(true));
  }

  // Hands
  const opp = $('opponents'); opp.innerHTML = '';
  s.players.filter(p => p.playerId !== myId).forEach(p => opp.appendChild(renderHand(p, { opponent: true })));
  const meRoot = $('me'); meRoot.innerHTML = '';
  if (mine) meRoot.appendChild(renderHand(mine));

  // Bottom buttons
  $('cambio').classList.toggle('hidden', !(s.phase === 'PLAYING' && myTurn && stage === 'DRAW'));
  $('newGame').classList.toggle('hidden', !finished);
}

/* ---------- networking + wiring ---------- */
const urlParams = new URLSearchParams(location.search);
const normCode = v => v.toUpperCase().replace(/[^A-Z0-9]/g, '');
let isHost = false;

function inviteLink(code) {
  const u = new URL(location.href);
  u.search = '';
  u.searchParams.set('room', code);
  const server = urlParams.get('server');
  if (server) u.searchParams.set('server', server);
  u.hash = '';
  return u.toString();
}

function setLobbyBusy(busy) {
  ['createGame', 'joinGame'].forEach(id => { $(id).disabled = busy; });
}

function showWaitingRoom(code) {
  $('lobbyEntry').classList.add('hidden');
  $('lobbyRoom').classList.remove('hidden');
  $('roomCode').textContent = code;
  // Make the address bar a shareable link to this exact game.
  try { history.replaceState(null, '', inviteLink(code)); } catch (_) {}
}

function renderPlayerList(players, myIsHost) {
  const ul = $('playerList');
  ul.innerHTML = '';
  players.forEach((n, i) => {
    const li = document.createElement('li');
    li.textContent = n;
    if (i === 0) {
      const tag = document.createElement('span');
      tag.className = 'host-tag';
      tag.textContent = 'host';
      li.appendChild(tag);
    }
    ul.appendChild(li);
  });
}

function handleMessage(e) {
  const m = JSON.parse(e.data);
  if (m.type === 'joined') {
    myId = m.playerId;
    showWaitingRoom(m.roomId);
    $('lobbyStatus').textContent = 'Waiting for players…';
  } else if (m.type === 'seat') {
    // Final seat number once the game starts (can differ if someone left the lobby).
    myId = m.playerId;
  } else if (m.type === 'lobby') {
    const n = m.playerCount;
    isHost = !!m.host;
    renderPlayerList(m.players || [], isHost);
    $('lobbyStatus').textContent = n < 2
      ? `Waiting for players… ${n}/2 minimum`
      : (isHost ? `${n} players joined. You can start the game.` : `${n} players joined. Waiting for the host.`);
    $('startGame').classList.toggle('hidden', !m.canStart);
  } else if (m.type === 'state') {
    const prevStage = state?.turnStage;
    state = m;
    trackReveals(m);
    if (m.flip) showFlip(m.flip);
    trackActivity(m.activity);
    if (m.turnStage !== 'AFTER_DRAW' || prevStage !== 'AFTER_DRAW') resetPower();
    $('lobby').classList.add('hidden');
    $('game').classList.remove('hidden');
    render();
    playAnimations();
  } else if (m.type === 'error') {
    flashMsg = '';
    setLobbyBusy(false);
    // A flip can lose a race with someone else's flip; stay quiet about it.
    if (Date.now() - lastFlipAttempt > 1500) toast(m.message);
  }
}

// Open the socket if needed, then send `msg` (create/join) once connected.
function connectAndSend(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) { send(msg); return; }
  if (ws && ws.readyState === WebSocket.CONNECTING) return;
  // Same origin as the page (works behind https tunnels/hosting too). Override
  // with ?server=host:port, e.g. when the page is served separately.
  const override = urlParams.get('server');
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const hostPart = override || location.host || 'localhost:8080';
  setLobbyBusy(true);
  ws = new WebSocket(`${proto}://${hostPart}`);
  $('connection').className = 'connection connecting';
  ws.onopen = () => {
    $('connection').className = 'connection connected';
    send(msg);
  };
  ws.onmessage = handleMessage;
  // Keep the connection alive through proxies/tunnels that drop idle sockets.
  const pingTimer = setInterval(() => send({ type: 'ping' }), 25000);
  ws.onclose = () => {
    clearInterval(pingTimer);
    $('connection').className = 'connection disconnected';
    setLobbyBusy(false);
  };
}

const playerName = () => $('name').value.trim() || 'Player';

$('createGame').onclick = () => {
  setLobbyBusy(true);
  connectAndSend({ type: 'create', name: playerName() });
};

function joinFromInput() {
  const code = normCode($('room').value);
  if (!code) { toast('Enter the game code your friend shared.'); return; }
  setLobbyBusy(true);
  connectAndSend({ type: 'join', roomId: code, name: playerName() });
}
$('joinGame').onclick = joinFromInput;
$('room').addEventListener('keydown', e => { if (e.key === 'Enter') joinFromInput(); });
$('room').addEventListener('input', () => { $('room').value = $('room').value.toUpperCase(); });

$('copyLink').onclick = async () => {
  const link = inviteLink($('roomCode').textContent);
  try {
    await navigator.clipboard.writeText(link);
    toast('Invite link copied!');
  } catch (_) {
    window.prompt('Copy this invite link:', link);
  }
};

// Opened via an invite link (?room=ABC123): prefill the code and ask for a name.
const invited = normCode(urlParams.get('room') || '');
if (invited) {
  $('room').value = invited;
  $('lobbyEntry').querySelector('.instruction').textContent = 'You’re invited to a game';
  $('name').focus();
}

$('startGame').onclick = () => send({ type: 'start' });
$('ready').onclick = () => action('CONFIRM_READY');
$('deck').onclick = () => action('DRAW_FROM_DECK');
$('discard').onclick = () => action('DRAW_FROM_DISCARD');
$('cambio').onclick = () => action('CALL_CAMBIO');
$('newGame').onclick = () => {
  // Back to the start screen; drop ?room= so we don't try to rejoin a finished game.
  const u = new URL(location.href);
  u.searchParams.delete('room');
  location.href = u.toString();
};
$('menuBtn').onclick = () => $('rulesModal').classList.remove('hidden');
$('closeRules').onclick = () => $('rulesModal').classList.add('hidden');
$('rulesModal').onclick = e => { if (e.target === $('rulesModal')) $('rulesModal').classList.add('hidden'); };
