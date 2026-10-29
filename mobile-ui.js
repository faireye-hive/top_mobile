(function (root) {
  'use strict';
  function directions(x, y) {
    const length = Math.hypot(x, y);
    if (length < 0.22) return [];
    const keys = [];
    if (x / length > 0.38) keys.push(100);
    if (x / length < -0.38) keys.push(97);
    if (y / length > 0.38) keys.push(115);
    if (y / length < -0.38) keys.push(119);
    return keys;
  }
  function viewport(width, height, mobile, quality) {
    width = Math.max(1, width); height = Math.max(1, height);
    const scale = Math.max((mobile ? 720 : 1280) / Math.max(1, width), (mobile ? 540 : 720) / Math.max(1, height), 1);
    const logicalWidth = Math.round(width * scale), logicalHeight = Math.round(height * scale);
    const mode = quality === 'auto' ? (mobile ? 'low' : 'balanced') : quality;
    const budget = mode === 'low' ? 960 * 540 : mode === 'high' ? 1600 * 900 : 1280 * 720;
    const renderScale = Math.min(1, Math.sqrt(budget / (logicalWidth * logicalHeight)));
    return {logicalWidth, logicalHeight, renderWidth: Math.max(1, Math.round(logicalWidth * renderScale)),
      renderHeight: Math.max(1, Math.round(logicalHeight * renderScale)), fps: mode === 'low' ? 30 : 60, mode};
  }
  class TouchInput {
    constructor(pointer, camera) { this.pointer = pointer; this.camera = camera; this.reset(); }
    reset() { this.touches = new Map(); this.drag = null; this.pinch = null; this.gesture = false; }
    pair() {
      const [a,b] = this.touches.values();
      return {x:(a.clientX+b.clientX)/2, y:(a.clientY+b.clientY)/2,
        distance:Math.hypot(a.clientX-b.clientX,a.clientY-b.clientY)};
    }
    down(event) {
      this.touches.set(event.pointerId,event);
      if (this.touches.size === 1) { this.drag={id:event.pointerId,start:event,dragging:false}; this.gesture=false; }
      else {
        if (this.drag && this.drag.dragging) this.pointer(3,event);
        this.drag=null; this.gesture=true;
        if (this.touches.size === 2) this.pinch=this.pair();
      }
    }
    move(event) {
      if (!this.touches.has(event.pointerId)) return;
      this.touches.set(event.pointerId,event);
      if (this.touches.size === 2) {
        const next=this.pair(), zoom=Math.trunc((next.distance-this.pinch.distance)/14);
        this.camera(Math.round(next.x-this.pinch.x),Math.round(next.y-this.pinch.y),zoom);
        this.pinch={...next,distance:zoom?next.distance:this.pinch.distance};
      } else if (this.drag && event.pointerId===this.drag.id && !this.gesture) {
        if (!this.drag.dragging && Math.hypot(event.clientX-this.drag.start.clientX,event.clientY-this.drag.start.clientY)>5) {
          this.pointer(0,this.drag.start); this.drag.dragging=true;
        }
        if (this.drag.dragging) this.pointer(1,event);
      }
    }
    end(event) {
      if (!this.touches.has(event.pointerId)) return;
      if (this.drag && this.drag.id===event.pointerId && !this.gesture) {
        if (event.type==='pointercancel' || event.type==='lostpointercapture') this.pointer(3,event);
        else { if (!this.drag.dragging) this.pointer(0,event); this.pointer(2,event); }
      }
      this.touches.delete(event.pointerId); this.drag=null;
      if (this.touches.size===2) this.pinch=this.pair();
      if (!this.touches.size) { this.gesture=false; this.pinch=null; }
    }
  }
  if (typeof module !== 'undefined' && module.exports) module.exports = {directions, viewport, TouchInput};
  if (!root.document) return;

  const M = root.Module, $ = id => document.getElementById(id), canvas = $('canvas');
  let settings = {quality: 'auto', controls: 'auto', diagnostics: false};
  try { settings = {...settings, ...JSON.parse(localStorage.getItem('top-web-options-v1') || '{}')}; } catch (_) {}
  if (!['auto','low','balanced','high'].includes(settings.quality)) settings.quality = 'auto';
  if (!['auto','touch','desktop'].includes(settings.controls)) settings.controls = 'auto';
  let running = false, ready = false, savePending = false, textQueue = [], previousName = '', nameStage = false, inWorld = false;
  let size, lastResume = 0, lastPresent = 0, nextPresent = 0, meterBegin = 0, meterFrames = 0, cpuSum = 0, cpuMax = 0;
  let statusTimer, sampledCpu = 0, sampledFps = 0;
  const held = new Set(), stickKeys = new Set();
  const persist = () => { try { localStorage.setItem('top-web-options-v1', JSON.stringify(settings)); } catch (_) {} };
  const mobileDevice = () => matchMedia('(pointer: coarse)').matches || navigator.maxTouchPoints > 0 || innerWidth < 900;
  const key = (code, down) => {
    if (!ready || !M._top_web_key) return;
    if (down) held.add(code); else held.delete(code);
    M._top_web_key(code, down ? 1 : 0);
  };
  const tap = code => { key(code, true); key(code, false); };
  function reset() {
    for (const code of Array.from(held)) key(code, false);
    stickKeys.clear(); gestureInput.reset();
    if (ready) M._top_web_pointer(3, 0, 0);
    stickPointer = null; cameraDrag = null;
    $('stick-thumb').style.transform = 'translate(0px, 0px)';
  }
  function resize() {
    M.topMobile = mobileDevice();
    if (M.topAssetStore) {
      M.topAssetStore.cacheBytes = (M.topMobile ? 96 : 192) * 1024 * 1024;
      M.topAssetStore.trim();
    }
    const touch = settings.controls === 'touch' || (settings.controls === 'auto' && M.topMobile);
    document.body.classList.toggle('touch-mode', touch);
    document.body.classList.toggle('is-mobile', M.topMobile);
    const rect = canvas.getBoundingClientRect();
    size = viewport(rect.width, rect.height, M.topMobile, settings.quality);
    M.topLogicalWidth = size.logicalWidth; M.topLogicalHeight = size.logicalHeight;
    if (canvas.width !== size.renderWidth || canvas.height !== size.renderHeight) {
      canvas.width = size.renderWidth; canvas.height = size.renderHeight;
    }
    $('quality-info').textContent = `${size.renderWidth} × ${size.renderHeight} · até ${size.fps} FPS`;
  }
  function status(text, persistent) {
    clearTimeout(statusTimer);
    $('status').textContent = text;
    $('status').hidden = !text;
    if (text && running && !persistent) statusTimer = setTimeout(() => { $('status').hidden = true; }, 6000);
  }
  M.setStatus = text => status(text, !ready);
  M.print = text => console.log(text);
  M.printErr = text => console.error(text);
  M.onAbort = reason => { console.error(reason); status('Nao foi possivel iniciar o jogo. Recarregue a pagina.', true); };
  M.onRuntimeInitialized = () => {
    $('start').disabled = false; $('start').textContent = 'Jogar';
    status('Pronto para jogar.');
  };
  M.topReady = () => {
    ready = true;
    document.body.classList.add('world-ready');
    $('loading').hidden = true;
    $('save').disabled = false;
    lastResume = performance.now();
    status(M.topMobile ? 'Toque no chão para andar. Use dois dedos para câmera e zoom.' : 'Clique para andar. I: bolsa · K: habilidades.');
  };
  M.topBeforeInput = () => {
    while (textQueue.length) M._top_web_text(textQueue.shift());
  };
  M.topNextFrame = async () => {
    const now = performance.now();
    if (lastResume && !document.hidden) {
      const work = now - lastResume;
      cpuSum += work; cpuMax = Math.max(cpuMax, work); meterFrames++;
      if (!meterBegin) meterBegin = now;
      if (now - meterBegin >= 1000) {
        sampledFps = meterFrames * 1000 / (now - meterBegin);
        sampledCpu = cpuSum / meterFrames;
        const stats = M.topAssetStats || {};
        $('perf-badge').textContent = `${sampledFps.toFixed(0)} FPS`;
        $('diagnostics').textContent = `${sampledFps.toFixed(1)} FPS · ativo ${sampledCpu.toFixed(1)} ms (max ${cpuMax.toFixed(1)})\n` +
          `${size.renderWidth} × ${size.renderHeight} · ${M.topDraws || 0} desenhos · ${M.topAttributes || 0} layouts / ${M.topUniforms || 0} uniforms\n` +
          `Arquivos em RAM: ${((stats.residentBytes || 0) / 1048576).toFixed(1)} MiB · ${stats.evictions || 0} liberados\n` +
          `Blocos recebidos: ${((stats.networkBytes || 0) / 1048576).toFixed(1)} MiB · ${stats.requests || 0} buscas · ${stats.diskHits || 0} do cache · ${stats.failures || 0} falhas`;
        meterBegin = now; meterFrames = cpuSum = cpuMax = 0;
      }
    }
    if (!!M.topInWorld !== inWorld) {
      inWorld = !!M.topInWorld;
      document.body.classList.toggle('in-world', inWorld);
    }
    if (!!M.topTextEntry !== nameStage) {
      nameStage = !!M.topTextEntry;
      $('text-entry').hidden = !nameStage || !M.topMobile;
      previousName = ''; textQueue = []; $('character-name').value = '';
    }
    await new Promise(resolve => {
      const tick = time => {
        if (document.hidden) {
          const resume = () => { if (!document.hidden) { document.removeEventListener('visibilitychange', resume); requestAnimationFrame(tick); } };
          document.addEventListener('visibilitychange', resume);
          return;
        }
        const interval = 1000 / size.fps;
        if (time >= nextPresent - 0.5) {
          if (time - lastPresent > 1000) nextPresent = time;
          nextPresent = Math.max(nextPresent + interval, time);
          lastPresent = time; resolve();
        } else requestAnimationFrame(tick);
      };
      requestAnimationFrame(tick);
    });
    lastResume = performance.now();
  };
  function point(event) {
    const rect = canvas.getBoundingClientRect();
    return {x: Math.round((event.clientX - rect.left) * M.topLogicalWidth / rect.width),
      y: Math.round((event.clientY - rect.top) * M.topLogicalHeight / rect.height)};
  }
  function pointer(phase, event) { const p = point(event); if (ready) M._top_web_pointer(phase, p.x, p.y); }
  const gestureInput = new TouchInput(pointer, (x,y,zoom) => M._top_web_camera(x,y,zoom));
  canvas.addEventListener('pointerdown', event => {
    if (event.pointerType === 'mouse') return;
    event.preventDefault();
    if (!ready) return;
    canvas.setPointerCapture(event.pointerId); gestureInput.down(event);
  });
  canvas.addEventListener('pointermove', event => {
    if (!gestureInput.touches.has(event.pointerId)) return;
    event.preventDefault(); gestureInput.move(event);
  });
  for (const name of ['pointerup','pointercancel','lostpointercapture']) canvas.addEventListener(name, event => {
    if (!gestureInput.touches.has(event.pointerId)) return;
    event.preventDefault(); gestureInput.end(event);
  });
  canvas.addEventListener('contextmenu', event => event.preventDefault());
  canvas.addEventListener('mousedown', () => canvas.focus({preventScroll: true}));

  for (const button of document.querySelectorAll('[data-key]')) {
    const code = Number(button.dataset.key);
    button.addEventListener('pointerdown', event => { event.preventDefault(); button.setPointerCapture(event.pointerId); key(code, true); });
    for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) button.addEventListener(name, () => key(code, false));
    button.addEventListener('click', event => { if (!event.detail) tap(code); });
  }
  const stick = $('joystick');
  let stickPointer = null;
  const moveStick = event => {
    const rect = stick.getBoundingClientRect(), radius = rect.width * .32;
    let x = (event.clientX - rect.left - rect.width / 2) / radius;
    let y = (event.clientY - rect.top - rect.height / 2) / radius;
    const length = Math.max(1, Math.hypot(x, y)); x /= length; y /= length;
    const next = new Set(directions(x, y));
    for (const code of stickKeys) if (!next.has(code)) key(code, false);
    for (const code of next) if (!stickKeys.has(code)) key(code, true);
    stickKeys.clear(); for (const code of next) stickKeys.add(code);
    $('stick-thumb').style.transform = `translate(${x * radius}px, ${y * radius}px)`;
  };
  stick.addEventListener('pointerdown', event => {
    if (stickPointer !== null) return;
    event.preventDefault(); stickPointer = event.pointerId; stick.setPointerCapture(event.pointerId); moveStick(event);
  });
  stick.addEventListener('pointermove', event => { if (event.pointerId === stickPointer) { event.preventDefault(); moveStick(event); } });
  const releaseStick = event => {
    if (event.pointerId !== stickPointer) return;
    for (const code of stickKeys) key(code, false);
    stickKeys.clear(); stickPointer = null; $('stick-thumb').style.transform = 'translate(0px, 0px)';
  };
  for (const event of ['pointerup','pointercancel','lostpointercapture']) stick.addEventListener(event, releaseStick);
  let cameraDrag = null;
  $('camera-pad').addEventListener('pointerdown', event => {
    event.preventDefault(); cameraDrag = event; $('camera-pad').setPointerCapture(event.pointerId);
  });
  $('camera-pad').addEventListener('pointermove', event => {
    if (cameraDrag && event.pointerId === cameraDrag.pointerId && ready) {
      M._top_web_camera(Math.round(event.clientX - cameraDrag.clientX), Math.round(event.clientY - cameraDrag.clientY), 0);
      cameraDrag = event;
    }
  });
  for (const name of ['pointerup','pointercancel','lostpointercapture']) $('camera-pad').addEventListener(name, () => { cameraDrag = null; });
  $('zoom-in').addEventListener('click', () => { if (ready) M._top_web_camera(0,0,1); });
  $('zoom-out').addEventListener('click', () => { if (ready) M._top_web_camera(0,0,-1); });
  $('menu-toggle').addEventListener('click', () => {
    reset(); $('menu').hidden = !$('menu').hidden;
    $('menu-toggle').setAttribute('aria-expanded', String(!$('menu').hidden));
  });
  $('menu-close').addEventListener('click', () => { $('menu').hidden = true; $('menu-toggle').setAttribute('aria-expanded','false'); canvas.focus(); });
  $('quality').value = settings.quality; $('controls').value = settings.controls;
  for (const id of ['quality','controls']) $(id).addEventListener('change', () => { settings[id] = $(id).value; reset(); persist(); resize(); });
  $('show-diagnostics').checked = !!settings.diagnostics;
  const diagnostics = () => { $('diagnostics').hidden = !settings.diagnostics; $('perf-badge').hidden = !settings.diagnostics; };
  $('show-diagnostics').addEventListener('change', () => { settings.diagnostics = $('show-diagnostics').checked; persist(); diagnostics(); });
  async function fullscreen() {
    try {
      if (document.fullscreenElement) await document.exitFullscreen();
      else if ($('app').requestFullscreen) await $('app').requestFullscreen({navigationUI:'hide'});
      else status('Use “Adicionar à Tela de Início” para abrir sem as barras do navegador.');
    } catch (_) { status('A tela já ocupa a área disponível. Você também pode adicionar o jogo à Tela de Início.'); }
    resize();
  }
  $('fullscreen').addEventListener('click', fullscreen);
  $('start').addEventListener('click', () => {
    if (running) return;
    running = true; $('start').disabled = true; $('loading-label').textContent = 'Abrindo o mundo…';
    document.body.classList.add('playing'); resize();
    if (M.topMobile) fullscreen();
    canvas.focus();
    const args = new URLSearchParams(location.search).has('profile') ? ['--profile','--startup-profile'] : [];
    M.callMain(args);
  });
  $('save').addEventListener('click', async () => {
    if (!ready || savePending) return;
    savePending = true; $('save').disabled = true; status('Salvando progresso…', true);
    const okay = await new Promise(resolve => { M.topSaveResolve = resolve; M.topSaveRequested = true; });
    status(okay ? 'Progresso salvo neste navegador.' : 'Nao foi possivel salvar. Tente novamente.', !okay);
    savePending = false; $('save').disabled = false;
  });
  $('character-name').addEventListener('input', () => {
    const value = $('character-name').value.replace(/[^a-zA-Z0-9 _-]/g, '').slice(0,24);
    $('character-name').value = value;
    // Replace the complete value in one native frame, including paste/delete.
    textQueue = [-1, ...Array.from(value, c => c.charCodeAt(0))];
    previousName = value;
  });
  $('character-name').addEventListener('keydown', event => { if (event.key === 'Enter') { event.preventDefault(); $('character-name').blur(); } });
  window.addEventListener('resize', () => { reset(); resize(); });
  document.addEventListener('fullscreenchange', () => { reset(); resize(); });
  window.addEventListener('blur', reset);
  document.addEventListener('visibilitychange', () => { if (document.hidden) reset(); });
  diagnostics(); resize();
})(globalThis);
