# axil-nd-fight

`nd-fight` for [axil-nd](../axil-nd), ported from SIC to libxylem.

Owns combat: weapon weights, attack construction, hit resolution, dodge, and
the `fighter` / `fighter_skel` tables. It is the chain base for
`on_will_attack` (seat stands the target up and passes the hit through,
spell amends it with strikes) and for `fighter_wt` (race supplies the racial
weight, equip the wielded weapon's), and the `hit_t` both chains carry is
defined in `<nd/fight.h>`.

## Install

```sh
make install
```

Installs:

```
lib/libnd-fight.so
include/nd/fight.h
```

There is deliberately no `lib/nd-fight.so` symlink (see `axil-nd-wts` for
why: `mods.load` names the installed filename, and the OpenBSD packing list
never lists a symlink).

Also packaged for deb, apk, rpm, brew and openbsd from a `v*` tag.

## Build from source

```sh
make
```

Needs [libxylem](https://github.com/tty-pt/libxylem) and the engine's game
API, `<nd/xy.h>`, plus the sibling headers it builds against (`<nd/attr.h>`,
`<nd/level.h>`, `<nd/mortal.h>`, `<nd/core.h>`) — from checkouts beside this
repo or from installed packages:

```sh
git clone https://github.com/tty-pt/nd-fight && cd nd-fight
git clone https://github.com/tty-pt/axil-nd ../axil-nd
git clone https://github.com/tty-pt/nd-attr ../axil-nd-attr
# ... and likewise nd-level, nd-mortal, nd-core
make
```

Both the checkout `-I` flags and the installed-package paths are on the
command line at once (see `Makefile`), and a missing `-I` is ignored, so the
same command works either way. CI names the deps explicitly
(`axil-nd,libxylem,nd-attr,nd-level,nd-mortal,nd-core`).

## What it does

* `xy_install()` registers the `fighter` and `fighter_skel` tables, the
  weapon-type vocabulary, and the attack commands.
* `fighter_attack` builds the hit, `dodge` resolves dodge attempts,
  `fight_damage` lands it; `fighter_wt`, `on_will_attack` and `on_hit` are
  the three chains this module both implements and calls, dispatched
  through its own registered adapters via small `_chain` helpers.
* Hooks with no implementor anywhere (`on_attack`, `on_did_attack`,
  `on_dodge`, `on_dodge_attempt`) stay `XY_DECL`-only in `<nd/fight.h>` —
  the dispatch finds nothing and returns 0.
* The old `on_icon` body is now a `core_icon_decorate` decorator registered
  in `xy_install` (the shop precedent).

## Testing

There is no `test.sh` here. Behaviour is asserted by the engine's own suite:

```sh
cd ../axil-nd
make && ./test.sh
```

## Notes from the port

* `SIC_DEF` → `XY_IMPL`, `mod_install` → `xy_install`, `call_f(...)` →
  `f(...)`. `call_verb`/`call_verb_to` never existed; attack messages go out
  as `nd_printf` + `nd_rwrite` via `OBJ.location`.
* Because this TU `XY_IMPL`s names from both `<nd/fight.h>` and
  `<nd/mortal.h>`, it defines `FIGHT_IMPL` and `MORTAL_IMPL` before including
  them. Co-implementors (seat, spell) define `FIGHT_IMPL` and manually
  re-declare the names they call but do not implement.
* `nd_printf` width handling needed bounded appends to satisfy
  `-Wformat-truncation` under the zero-warnings rule.
* The link line is libxylem alone. `NEEDED` is `libxylem.so` and `libc.so.6`.

## License

BSD 2-Clause, carried over from `tty-pt/nd-fight`. See `LICENSE`.
