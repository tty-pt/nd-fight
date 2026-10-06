## 1.0.2

- **The sibling `-I` lines are gone.** `CFLAGS += -I$(shell cd .. && pwd)/…`
  pointed at the axil-nd/nd sibling checkouts and only existed for a dev
  build: in CI those directories do not exist and every header comes from the
  installed packages named in `.github/workflows/ci.yml`. The build now
  resolves `<nd/…>` the way a packager sees it.
- **macOS: link with `-undefined dynamic_lookup`.** macOS `ld` rejects
  undefined symbols in a shared library, but `WARN` needs `qsyslog` — an
  engine-provided function pointer resolved at `dlopen` time (Linux allows
  this by default). `-undefined dynamic_lookup` is the Darwin equivalent, set
  as `LDFLAGS-libnd-fight-Darwin` so no other platform is affected.

## [1.0.0]

- **nd-fight is now an installable library rather than a build artifact of
  the engine.** It builds and installs exactly two files,
  `lib/libnd-fight.so` and `include/nd/fight.h`, following the same layout as
  `axil-tty` and `axil-auth`, and the same layout `nd-core` was converted to
  first. Previously `make` produced a `fight.so` named by the engine's
  `mods.load` and installed nothing. There is no `lib/nd-fight.so` symlink:
  `mods.load` names this module `libnd-fight`, the installed filename, and
  `module_load_path()` only appends `.so`.

- **The link line is libxylem alone.** `LDLIBS := -lxylem`; the engine is not
  linked. `NEEDED` is `libxylem.so` and `libc.so.6`.

- **Combat chains are declared in `<nd/fight.h>`.** `fighter_wt`,
  `on_will_attack`, `on_hit` (plus `fight_damage`, `fighter_attack`,
  `fighter_target`, `fighter_skel_add`, and the DECL-only `on_attack`,
  `on_did_attack`, `on_dodge`, `on_dodge_attempt`) are `XY_DECL`'d there, and
  the `hit_t` the chains carry is defined there too. This TU defines
  `FIGHT_IMPL` (and `MORTAL_IMPL`) because it `XY_IMPL`s the same names.
  `fighter_wt`, `on_will_attack` and `on_hit` are both implemented and called
  here, so they go through this module's own registered adapters via small
  `_chain` helpers.

- **`call_verb`/`call_verb_to` never existed** and are gone from the port;
  attack messages go out as `nd_printf` + `nd_rwrite` via `OBJ.location`.
  The old `on_icon` body is now a `core_icon_decorate` decorator.

- **Dropped the `nd-mod.mk` dependency.** `nd-mod.mk` has now been deleted.
