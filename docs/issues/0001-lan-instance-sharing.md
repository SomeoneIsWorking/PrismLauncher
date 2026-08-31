---
id: 1
title: LAN instance sharing
status: investigating
symptom: A player cannot directly share a Prism instance with another launcher on the same local network
state_items: S003,S004,S005,S006
tags: lan,instance-sharing,import,export
created: 2026-08-31
updated: 2026-08-31
---

## Root cause

Prism has independent archive export and URL-based import owners, but no
cohesive local-network offer lifecycle that connects them.

## What was tried / dead ends

No existing direct-local-network owner was present; the implementation reuses
the established archive and import owners instead of adding a second format.

## Resolution

`Lan::Offer` serves only a temporary archive behind a 256-bit capability, and
the recipient delegates the validated URL to `InstanceImportTask`.
