---
id: 2
title: Direct desktop shortcut action
status: implementing
symptom: Creating a desktop shortcut asks for a name and then asks for a filename despite the instance already having a name
state_items: S011
tags: shortcut,desktop,ui
created: 2026-08-31
updated: 2026-08-31
---

## Root cause

The selected-instance shortcut action delegated to a general shortcut dialog
that supports target, account, quick-play, icon, and destination choices. Its
desktop path therefore prompted twice for a simple instance shortcut.

## Resolution

The selected-instance action now directly invokes the established desktop
shortcut writer using the selected instance name, and the unused general dialog
is removed.
