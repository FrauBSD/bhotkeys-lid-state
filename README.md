[//]: # ($FrauBSD: bhotkeys-lid-state/README.md 2026-10-04 21:30:41 -0700 Devin Teske $)

# bhotkeys-lid-state

`Super+Z` chooses whether closing the lid sleeps the machine or
leaves it awake.

One [bhotkeys](https://github.com/FrauBSD/bhotkeys) plugin. This
package ships `lid-wake-toggle`, `lid-policy`, and `lid-switchd`.
`lid-switchd` is the root helper that sets `hw.acpi.lid_switch_state`.
`lid-policy` asks it to change that policy and, on success, shows
the new state with [bosd](https://github.com/FrauBSD/bosd).
The plugin is available at the greeter, so a laptop left closed
on a desk can be told to stay up before anyone logs in.

Home: [FrauBSD/bhotkeys-lid-state](https://github.com/FrauBSD/bhotkeys-lid-state)

## Requirements

- `bhotkeys`
- `bosd` for the lid glyph
- `lid-switchd` enabled (`sysrc lid_switchd_enable="YES"`)

The socket is mode 0660 and group `lid_switchd`. The service
creates that group. Add the session account to it, then log in
again so the new group is in the credentials:

```sh
pw groupmod lid_switchd -m user
```

## Build / install

```sh
make install    # PREFIX=/usr/local by default
```

Installs `lid-wake-toggle` and `lid-policy` into
`${PREFIX}/bin`, `lid-switchd` into `${PREFIX}/sbin`,
its rc script into `${PREFIX}/etc/rc.d`, and `lid` into
`${PREFIX}/share/bhotkeys/plugins.d`. The glyphs are
`lid-awake` and `lid-s0ix` from bosd.

## Plugin

```
id lid
label Lid policy
chord Super+z
command lid-wake-toggle
greeter 1
```
