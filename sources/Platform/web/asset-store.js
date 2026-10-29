/* Compressed, content-addressed resources. C++ FILE handles pin their data. */
(function (root) {
  'use strict';
  class TopAssetStore {
    constructor(options) {
      this.fs = options.fs;
      this.path = options.path;
      this.index = options.index;
      this.baseURL = options.baseURL;
      this.fetch = options.fetch || root.fetch.bind(root);
      this.cacheBytes = options.cacheBytes || 128 * 1024 * 1024;
      this.onProgress = options.onProgress || (() => {});
      this.assets = new Map();
      this.packFiles = new Map();
      this.resident = new Map();
      this.pins = new Map();
      this.inflight = new Map();
      this.diskResponses = new WeakSet();
      this.stats = {requests: 0, networkBytes: 0, diskHits: 0, residentBytes: 0, peakResidentBytes: 0,
                    filesLoaded: 0, evictions: 0, failures: 0, pending: 0};
      if (![1, 2].includes(this.index.version) || !Array.isArray(this.index.files)) throw Error('Indice invalido');
      this.cache = options.cacheStorage ? options.cacheStorage.open('top-resource-packs-v2').catch(() => null) : Promise.resolve(null);
      for (const entry of this.index.files) {
        if (typeof entry.path !== 'string' || !entry.path.startsWith('Client/') ||
            entry.path.split('/').some(p => !p || p === '.' || p === '..') ||
            entry.path === 'Client/user' || entry.path.startsWith('Client/user/') ||
            !Number.isSafeInteger(entry.size) || entry.size < 0) throw Error('Caminho de recurso invalido');
        const path = '/' + entry.path;
        if (this.assets.has(path)) throw Error('Recurso duplicado');
        this.assets.set(path, entry);
        this.fs.mkdirTree(this.path.dirname(path));
        this.fs.writeFile(path, new Uint8Array(0));
        if (this.index.version === 2) {
          const pack = this.index.packs[entry.pack];
          if (!pack || !/^packs\/[a-f0-9]{64}\.pack$/.test(pack.url) ||
              !Number.isSafeInteger(entry.offset) || entry.offset < 0 ||
              entry.offset + entry.size > pack.unpackedSize) throw Error('Bloco invalido');
          if (!this.packFiles.has(entry.pack)) this.packFiles.set(entry.pack, []);
          this.packFiles.get(entry.pack).push(path);
        }
      }
    }
    normalize(path) {
      return this.path.normalize((path.startsWith('/') ? '' : this.fs.cwd() + '/') + path);
    }
    touch(path) {
      if (!this.resident.has(path)) return;
      const bytes = this.resident.get(path);
      this.resident.delete(path);
      this.resident.set(path, bytes);
    }
    pin(rawPath) {
      const path = this.normalize(rawPath);
      if (!this.assets.has(path)) return false;
      this.pins.set(path, (this.pins.get(path) || 0) + 1);
      this.touch(path);
      return true;
    }
    unpin(rawPath) {
      const path = this.normalize(rawPath), count = this.pins.get(path) || 0;
      if (count <= 1) this.pins.delete(path);
      else this.pins.set(path, count - 1);
      this.trim();
    }
    needs(rawPath) {
      const path = this.normalize(rawPath);
      return this.assets.has(path) && !this.resident.has(path);
    }
    install(path, bytes) {
      if (this.resident.has(path)) return; // Never replace data behind an open FILE.
      this.fs.writeFile(path, bytes);
      this.resident.set(path, bytes.byteLength);
      this.stats.residentBytes += bytes.byteLength;
      this.stats.peakResidentBytes = Math.max(this.stats.peakResidentBytes, this.stats.residentBytes);
      this.stats.filesLoaded++;
    }
    trim() {
      for (const [path, bytes] of this.resident) {
        if (this.stats.residentBytes <= this.cacheBytes) break;
        if (this.pins.has(path)) continue;
        this.fs.writeFile(path, new Uint8Array(0));
        this.resident.delete(path);
        this.stats.residentBytes -= bytes;
        this.stats.evictions++;
      }
    }
    async response(url, integrity) {
      const cache = await this.cache;
      if (cache) {
        const cached = await cache.match(url).catch(() => null);
        if (cached) { this.stats.diskHits++; this.diskResponses.add(cached); return cached; }
      }
      this.stats.requests++;
      const response = await this.fetch(url, {integrity: 'sha256-' + integrity});
      if (!response.ok) throw Error('HTTP ' + response.status);
      if (cache) cache.put(url, response.clone()).catch(() => {});
      return response;
    }
    async loadPack(number) {
      const pack = this.index.packs[number];
      if (!root.DecompressionStream) throw Error('Atualize o navegador para carregar os recursos compactados');
      const response = await this.response(new URL(pack.url, this.baseURL).href, pack.sha256);
      const encoded = await response.arrayBuffer();
      if (encoded.byteLength !== pack.size) throw Error('Bloco incompleto');
      if (!this.diskResponses.has(response)) this.stats.networkBytes += encoded.byteLength;
      const stream = new Blob([encoded]).stream().pipeThrough(new DecompressionStream('gzip'));
      const bytes = new Uint8Array(await new Response(stream).arrayBuffer());
      if (bytes.byteLength !== pack.unpackedSize) throw Error('Tamanho do bloco invalido');
      for (const path of this.packFiles.get(number)) {
        const entry = this.assets.get(path);
        this.install(path, bytes.subarray(entry.offset, entry.offset + entry.size));
      }
    }
    async ensure(rawPath) {
      const path = this.normalize(rawPath), entry = this.assets.get(path);
      if (!entry || this.resident.has(path)) { this.touch(path); return true; }
      const key = this.index.version === 2 ? entry.pack : path;
      this.stats.pending++;
      this.onProgress(entry.path, this.stats);
      try {
        if (!this.inflight.has(key)) {
          const work = this.index.version === 2 ? this.loadPack(key) : (async () => {
            const url = new URL('assets/' + entry.path.split('/').map(encodeURIComponent).join('/') +
              '?v=' + encodeURIComponent(entry.sha256), this.baseURL);
            const response = await this.response(url.href, entry.sha256);
            const bytes = new Uint8Array(await response.arrayBuffer());
            if (bytes.length !== entry.size) throw Error('Recurso incompleto');
            if (!this.diskResponses.has(response)) this.stats.networkBytes += bytes.byteLength;
            this.install(path, bytes);
          })();
          this.inflight.set(key, work);
        }
        await this.inflight.get(key);
        this.touch(path);
        this.trim();
        return true;
      } catch (error) {
        this.stats.failures++;
        this.onProgress(entry.path, this.stats, error);
        return false;
      } finally {
        this.inflight.delete(key);
        this.stats.pending--;
      }
    }
  }
  root.TopAssetStore = TopAssetStore;
  if (typeof module !== 'undefined' && module.exports) module.exports = TopAssetStore;
})(globalThis);
