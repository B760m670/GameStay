// Браузер: 1) сохранения — /save в IndexedDB, читаем его до старта игры;
// 2) ресурсы — по списку assets.json скачиваем файлы рядом со страницей и кладём в /assets.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  FS.mkdir('/save');
  FS.mount(IDBFS, {}, '/save');
  addRunDependency('save-sync');
  FS.syncfs(true, function (err) {
    if (err) console.warn('load saves', err);
    removeRunDependency('save-sync');
  });

  addRunDependency('assets');
  fetch('assets.json').then(function (r) { return r.json(); }).then(function (list) {
    var total = list.reduce(function (s, f) { return s + f.size; }, 0), done = 0;
    var status = function () { if (Module.setStatus) Module.setStatus('Downloading data... (' + done + '/' + total + ')'); };
    status();
    var queue = list.slice(), workers = [];
    function next() {
      var f = queue.shift();
      if (!f) return Promise.resolve();
      return fetch(f.path).then(function (r) {
        if (!r.ok) throw new Error(f.path + ': ' + r.status);
        return r.arrayBuffer();
      }).then(function (buf) {
        var full = '/' + f.path;
        FS.mkdirTree(full.substring(0, full.lastIndexOf('/')));
        FS.writeFile(full, new Uint8Array(buf));
        done += f.size;
        status();
        return next();
      });
    }
    for (var i = 0; i < 4; i++) workers.push(next());  // четыре загрузки параллельно
    return Promise.all(workers);
  }).then(function () {
    removeRunDependency('assets');
  }).catch(function (e) {
    if (Module.onAbort) Module.onAbort('не удалось скачать ресурсы (' + e.message + ')');
  });
});
