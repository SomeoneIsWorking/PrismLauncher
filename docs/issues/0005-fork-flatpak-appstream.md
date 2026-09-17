---
id: 5
title: Fork Flatpak repository publishes no appstream metadata
status: resolved
symptom: flatpak upgrade reports Error updating appstream2/appstream: No such ref in remote prism-fork-local
state_items: S012
tags: flatpak,appstream,packaging
created: 2026-09-17
updated: 2026-09-17
---

## Root cause

The `prism-fork-local` repository was built with only
`flatpak build-import-bundle`, which imports the app commit and refreshes the
summary but does not generate the `appstream2/x86_64` branch. Flatpak 1.18.2
has no per-remote "no appstream" switch: `flatpak update`/`upgrade` without an
explicit ref calls `update_appstream` for every enabled, enumerable remote whose
appstream timestamp is stale. With no timestamp file the age is `G_MAXUINT64`,
so `prism-fork-local` was retried every time and failed with
`No such ref 'appstream2/x86_64' in remote` plus the v1 fallback error. The same
repository generation gap also left the branch stale after every in-app update,
because `FlatpakUpdate::installBundle` also stopped after the import.

## What was tried / dead ends

`--no-enumerate` is the only flatpak flag that skips the appstream attempt, but
it changes enumeration and `remote-info` semantics for the self-updater, so it
treats the symptom rather than the missing repository metadata.

## Resolution

`flatpak build-import-bundle` is now followed by `flatpak build-update-repo`
everywhere a bundle enters the repository: the one-time setup tool
(`tools/setup_fork_flatpak.py::import_bundle`) and the in-app updater
(`FlatpakUpdate::installBundle`). `build-update-repo` is the canonical step that
generates the appstream branch and republishes the summary. The live
`prism-fork-local` repository was repaired in place with the same command.

## Evidence

An isolated `FLATPAK_USER_DIR` reproduced the exact error against the
pre-repair repository and ran silently against a `build-update-repo` copy, with
identical flatpak and remote configuration otherwise. After repairing the live
repository, `flatpak --user update --appstream` deployed
`~/.local/share/flatpak/appstream/prism-fork-local/x86_64/` and its `.timestamp`
with no appstream error. `tests/test_flatpak_setup.py` asserts that the
repository import is followed by `build-update-repo`.
