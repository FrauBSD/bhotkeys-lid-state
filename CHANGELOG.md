[//]: # ($FrauBSD: bhotkeys-lid-state/CHANGELOG.md 2026-10-09 13:49:18 -0700 Devin Teske $)

# Changelog

Newest first. Each section is a git tag; the bullets are what landed
in that tag (from the previous tag, or from the start of the
repository for 1.0).

## 1.3 (2026-10-09)

- installing the package creates group `lid_switchd`; the rc
  script no longer creates it

## 1.2 (2026-10-05)

- man pages for `lid-wake-toggle`, `lid-policy`, and `lid-switchd`
- `lid-switchd` accepts either `suspend_to_idle` or `s2idle` and applies the
  name `kern.power.supported_stype` lists
- at start, `lid-switchd` keeps the live `hw.acpi.lid_switch_state`; the boot
  value belongs in `sysctl.conf`
- `lid_switchd_switch_state` chooses the toggle sleep side, `s0ix` or `s3`;
  `s3` uses `fw_suspend` (`s2mem` on older kernels)

## 1.1 (2026-10-04)

- `lid-switchd` socket group is `lid_switchd`, not `wheel`

## 1.0 (2026-10-03)

- `lid` plugin: Super+Z runs lid-wake-toggle; offered at the greeter
