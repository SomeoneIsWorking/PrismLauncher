---
id: 4
title: Verify Java runtime management
status: resolved
symptom: Project state does not yet prove the current packaged launcher's user-facing Java runtime selection, validation, and persistence workflow
state_items: S007
tags: java,settings,flatpak,verification
created: 2026-09-04
updated: 2026-09-04
---

## Root cause

The implementation was present, but the project-state inventory had no
user-facing evidence from the current packaged release. Source presence alone
did not establish that the Flatpak could execute the configured runtime or
complete its managed-runtime lifecycle.

## What was tried / dead ends

An existing Java path was supplied only to seed the isolated test root. The
actual validation, managed download, discovery, removal, and settings write
were all performed through the packaged launcher's UI.

## Resolution

The GitHub-built 12.0.7 Flatpak from commit `2960f237d` validated Microsoft
Java 17.0.15 (`amd64`) both normally and with the configured 4 GiB heap. Its
Installations page then downloaded Mojang `java-runtime-gamma` 17.0.15 into an
isolated launcher root, discovered and displayed it, and removed it after the
confirmation flow. Changing maximum memory from 4096 MiB to 3072 MiB through
the same settings page persisted `MaxMemAlloc=3072` in that isolated
configuration.
