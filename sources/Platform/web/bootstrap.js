// All resources are public game data. Client/user is a separate IDBFS mount;
// packaging must never publish the developer's local saves.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  addRunDependency('top-resources');
  (async function () {
    const response = await fetch('assets-index.json.gz', {cache: 'no-cache'});
    if (!response.ok) throw new Error('Indice de recursos indisponivel: HTTP ' + response.status);
    if (!globalThis.DecompressionStream) throw new Error('Atualize o navegador para carregar o jogo');
    const index = await new Response(response.body.pipeThrough(new DecompressionStream('gzip'))).json();
    const store = new TopAssetStore({fs: FS, path: PATH, index, baseURL: document.baseURI,
      cacheBytes: (Module.topMobile ? 96 : 192) * 1024 * 1024,
      cacheStorage: globalThis.caches,
      onProgress(path, stats, error) {
        if (error) {
          console.error('Recurso nao carregado:', path, error);
          Module.setStatus('Falha ao carregar recurso. Verifique a conexao e tente novamente.');
        } else if (!Module.topFirstFrame) Module.setStatus('Carregando o mundo…');
      }
    });
    Module.topAssetStore = store;
    Module.topAssetStats = store.stats;
    Module.topNeedsAsset = path => store.needs(path);
    Module.topEnsureAsset = async path => (await store.ensure(path)) ? 1 : 0;
    Module.topPinAsset = path => store.pin(path) ? 1 : 0;
    Module.topUnpinAsset = path => store.unpin(path);

    const scope = new URL('.', document.baseURI).pathname;
    // Keep the existing localhost saves. GitHub project sites share an origin,
    // so each project subdirectory gets its own persistent mount/database.
    const saveMount = scope === '/' ? '/Client/user' : '/top-saves' + scope.replace(/\/$/, '');
    FS.mkdirTree(saveMount);
    FS.mount(IDBFS, {}, saveMount);
    if (saveMount !== '/Client/user') FS.symlink(saveMount, '/Client/user');
    await new Promise((resolve, reject) => FS.syncfs(true, error => error ? reject(error) : resolve()));
    let nextFlush = 0, flushing = null;
    Module.topFlushSaves = async function (force) {
      if (flushing) return flushing;
      if (!force && performance.now() < nextFlush) return;
      nextFlush = performance.now() + 5000;
      flushing = new Promise((resolve, reject) => FS.syncfs(false, error => error ? reject(error) : resolve()));
      try {
        await flushing;
        Module.topSaveError = null;
      } catch (error) {
        Module.topSaveError = String(error);
        console.error('Falha ao persistir saves:', error);
        Module.setStatus('Nao foi possivel salvar neste navegador. Mantenha a aba aberta e tente novamente.');
      } finally { flushing = null; }
    };
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) Module.topFlushSaves(true);
    });
    Module.topResourcesReady = true;
    removeRunDependency('top-resources');
  })().catch(error => {
    console.error(error);
    Module.setStatus('Falha ao preparar o jogo: ' + error.message);
    // Do not start with an empty save mount after a failed IndexedDB restore.
  });
});
