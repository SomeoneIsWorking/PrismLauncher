---
id: 2
title: Direct desktop shortcut action
status: resolved
symptom: Creating a desktop shortcut asks for a name and then asks for a filename despite the instance already having a name
state_items: S011
tags: shortcut,desktop,ui
created: 2026-08-31
updated: 2026-09-04
---

## Root cause

The selected-instance shortcut action delegated to a general shortcut dialog
that supports target, account, quick-play, icon, and destination choices. Its
desktop path therefore prompted twice for a simple instance shortcut. The
Flatpak also lacked access to `xdg-desktop`, so its direct shortcut writer
could not create the resulting desktop entry even after the dialog was
removed.

## Resolution

The selected-instance action now directly invokes the established desktop
shortcut writer using the selected instance name, and the unused general dialog
is removed. The Flatpak grants read-write access only to the standard desktop
directory needed by that writer.

## Evidence

On 2026-09-04, the unmodified installed Flatpak denied writes to the desktop
and the same binary allowed them with `xdg-desktop:create`, discriminating the
missing manifest grant. A packaged Flatpak containing the grant then created
`Codex Shortcut Verification.desktop` with one click, used the instance name,
targeted `flatpak run org.prismlauncher.PrismLauncher --launch shortcut-test`,
registered the shortcut with the source instance, and presented only the
success message rather than the retired naming or save dialogs.
