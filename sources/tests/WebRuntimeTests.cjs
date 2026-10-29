const {test} = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path').posix;
const {gzipSync} = require('node:zlib');
const {createHash} = require('node:crypto');
const AssetStore = require('../Platform/web/asset-store.js');
const {directions, viewport, TouchInput} = require('../Platform/web/mobile-ui.js');

function fixture(cacheBytes = 100) {
  const content = new Map(), original = Buffer.from('AAAABBBBCCCC');
  const packed = gzipSync(original), digest = createHash('sha256').update(packed);
  const hex = digest.copy().digest('hex'), hash = digest.digest('base64');
  const index = {version: 2, packs: [{url: `packs/${hex}.pack`, size: packed.length,
    unpackedSize: original.length, sha256: hash}], files: ['a','b','c'].map((name, i) =>
    ({path: `Client/model/${name}.lgo`, size: 4, offset: i * 4, pack: 0}))};
  const disk = new Map();
  const cacheStorage = {open: async () => ({match: async url => disk.get(url)?.clone(),
    put: async (url, response) => { disk.set(url, response); }})};
  let requests = 0, fail = false;
  const options = {index, cacheBytes, path, baseURL: 'https://example.test/my-game/', cacheStorage,
    fs: {cwd: () => '/', mkdirTree() {}, writeFile: (p, bytes) => content.set(p, Buffer.from(bytes))},
    fetch: async (url, opts) => {
      requests++;
      assert.equal(url, 'https://example.test/my-game/' + index.packs[0].url);
      assert.equal(opts.integrity, 'sha256-' + hash);
      return fail ? new Response('unavailable', {status: 503}) : new Response(packed);
    }};
  return {store: new AssetStore(options), options, content, count: () => requests,
    fail: value => { fail = value; }};
}

test('bundled C++ readers share one fetch and keep byte-identical data', async () => {
  const f = fixture();
  f.store.pin('Client/model/a.lgo'); f.store.pin('Client/model/b.lgo');
  assert.deepEqual(await Promise.all([f.store.ensure('Client/model/a.lgo'), f.store.ensure('Client/model/b.lgo')]), [true,true]);
  assert.equal(f.count(), 1);
  assert.equal(f.content.get('/Client/model/a.lgo').toString(), 'AAAA');
  assert.equal(f.content.get('/Client/model/b.lgo').toString(), 'BBBB');
});
test('LRU never evicts an open FILE, and can restore an evicted file from disk cache', async () => {
  const f = fixture(4);
  f.store.pin('Client/model/a.lgo');
  assert.equal(await f.store.ensure('Client/model/a.lgo'), true);
  assert.equal(f.store.stats.residentBytes, 4);
  assert.equal(f.content.get('/Client/model/a.lgo').toString(), 'AAAA');
  assert.equal(f.store.needs('Client/model/c.lgo'), true);
  f.store.pin('Client/model/c.lgo');
  assert.equal(await f.store.ensure('Client/model/c.lgo'), true);
  assert.equal(f.store.stats.residentBytes, 8); // Both FILE handles are pinned.
  f.store.unpin('Client/model/a.lgo');
  assert.equal(f.store.stats.residentBytes, 4);
  assert.equal(f.store.needs('Client/model/a.lgo'), true);
  assert.equal(f.content.get('/Client/model/c.lgo').toString(), 'CCCC');
  assert.equal(f.count(), 1); assert.equal(f.store.stats.diskHits, 1);
});
test('a failed download remains retryable and never becomes an empty success', async () => {
  const f = fixture(); f.store.pin('Client/model/a.lgo'); f.fail(true);
  assert.equal(await f.store.ensure('Client/model/a.lgo'), false);
  assert.equal(f.store.needs('Client/model/a.lgo'), true);
  f.fail(false);
  assert.equal(await f.store.ensure('Client/model/a.lgo'), true);
  assert.equal(f.count(), 2); assert.equal(f.store.stats.failures, 1);
});
test('corrupt pack offsets and private-save paths are rejected', () => {
  const f = fixture();
  assert.throws(() => new AssetStore({...f.options, index: {...f.options.index,
    files: [{path:'Client/user/save.txt',size:4,offset:0,pack:0}]}}));
  assert.throws(() => new AssetStore({...f.options, index: {...f.options.index,
    files: [{path:'Client/model/a',size:50,offset:0,pack:0}]}}));
});
test('a new session can use persistent packs without a network request', async () => {
  const f = fixture(); f.store.pin('Client/model/a.lgo'); await f.store.ensure('Client/model/a.lgo');
  const next = new AssetStore({...f.options, fetch: async () => { throw Error('offline'); }});
  next.pin('Client/model/a.lgo');
  assert.equal(await next.ensure('Client/model/a.lgo'), true);
  assert.equal(next.stats.requests, 0); assert.equal(next.stats.diskHits, 1);
});
test('virtual stick uses the shared WASD movement with a dead zone', () => {
  assert.deepEqual(directions(.1,0), []);
  assert.deepEqual(directions(0,-1), [119]);
  assert.deepEqual(directions(1,1), [100,115]);
  assert.deepEqual(directions(-1,-1), [97,119]);
});
test('touch tap and inventory drag produce the native pointer sequence', () => {
  const events=[], gesture=new TouchInput((phase,event)=>events.push([phase,event.clientX]),()=>{});
  const e=(x,type='pointermove')=>({pointerId:1,clientX:x,clientY:10,type});
  gesture.down(e(10)); gesture.end(e(10,'pointerup'));
  assert.deepEqual(events,[[0,10],[2,10]]);
  events.length=0;
  gesture.down(e(10)); gesture.move(e(25)); gesture.end(e(30,'pointerup'));
  assert.deepEqual(events,[[0,10],[1,25],[2,30]]);
});
test('pinch and cancellation cannot accidentally click or walk', () => {
  const clicks=[], camera=[], gesture=new TouchInput((phase)=>clicks.push(phase),(...args)=>camera.push(args));
  const a={pointerId:1,clientX:10,clientY:10}, b={pointerId:2,clientX:50,clientY:10};
  gesture.down(a);gesture.down(b);gesture.move({...b,clientX:80});
  gesture.end({...a,type:'pointerup'});gesture.end({...b,type:'pointerup'});
  assert.deepEqual(clicks,[]);assert.equal(camera[0][2],2);
  gesture.down(a);gesture.end({...a,type:'pointercancel'});
  assert.deepEqual(clicks,[3]);assert.equal(gesture.touches.size,0);
});
test('portrait and landscape retain aspect and bound render pixels without shrinking UI below its modal size', () => {
  for (const [w,h] of [[390,844],[844,390],[1280,720],[0,0]]) {
    const v = viewport(w,h,true,'auto');
    assert.ok(v.logicalWidth >= 720 && v.logicalHeight >= 540);
    assert.ok(v.renderWidth * v.renderHeight <= 960 * 540 + 2000);
    assert.equal(v.fps, 30);
    if (w && h) assert.ok(Math.abs(v.logicalWidth / v.logicalHeight - w / h) < .003);
  }
  assert.equal(viewport(1280,720,false,'auto').fps, 60);
});
