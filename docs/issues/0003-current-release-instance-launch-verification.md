---
id: 3
title: Current release instance launch verification
status: resolved
symptom: The project state lacks current end-to-end evidence that the released launcher starts an existing instance with its declared Minecraft components
state_items: S001
tags: launch,flatpak,verification
created: 2026-09-04
updated: 2026-09-04
---

## Root cause

The state inventory only cited subsystem presence. It had no current packaged
run proving that the launcher resolved an existing instance, applied its
declared components, started its configured Java runtime, and reached a usable
Minecraft client.

## What was tried / dead ends

Source inspection and an old instance log were not accepted as current release
evidence. The verification instead used the exact Flatpak artifact built from
the release tag.

## Resolution

The GitHub-built 12.0.7 Flatpak selected `Supermarket-pack`, applied its LWJGL,
Minecraft, intermediary, and Fabric Loader components, launched the configured
Java 17 runtime, and reached the rendered Minecraft 1.20.1/Fabric main menu.
The run used a hidden display and OpenAL's `No Output` device, then exited
Minecraft through its Quit action and recorded a clean `Stopping!` event.
