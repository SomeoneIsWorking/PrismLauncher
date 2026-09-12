---
id: 2
title: Update an existing instance from LAN
status: resolved
symptom: LAN transfer can only create a new instance, so a recipient cannot update an installed modpack from a peer
state_items: S015
tags: lan,instance-update,import
created: 2026-09-12
updated: 2026-09-12
---

## Root cause

The LAN page handed an archive URL only to the New Instance importer. The
existing staged-instance override merged files in place, which would retain
removed mods and could leave a partial update after a failed copy.

## Resolution

The receiver can choose a stopped local Minecraft instance and use the
existing LAN transfer and archive importer to stage the peer's instance. The
LAN update owner preserves local player data, validates the staged profile,
then swaps the directory with rollback on a failed install. The full evidence
and current capability state are recorded in S015.
