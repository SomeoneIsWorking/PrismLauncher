---
id: 1
title: LAN instance sharing
status: resolved
symptom: A player cannot directly share a Prism instance with another launcher on the same local network
state_items: S003,S004,S005,S006
tags: lan,instance-sharing,import,export
created: 2026-08-31
updated: 2026-09-04
---

## Root cause

Prism had independent archive export and URL-based import owners, but no
application-owned local-network lifecycle connecting them. The incomplete
first implementation also depended on an explicit sender share dialog, routed
discovery and control traffic through the same shared UDP socket, and described
a 256-bit capability while generating only 128 bits.

## What was tried / dead ends

No existing direct-local-network owner was present; the implementation reuses
the established archive and import owners instead of adding a second format.

## Resolution

`Lan::InstanceService` now advertises every instance automatically, uses a
fixed socket for discovery and a per-launcher ephemeral socket for control,
prepares non-running instances only when requested, and serves each temporary
archive through `Lan::Offer` behind a generated 256-bit capability. The New
Instance window owns an `Import from LAN` page and delegates the prepared URL
to `InstanceImportTask`. Focused protocol/service/offer tests and a real
two-launcher discovery-to-import run verify the completed boundary.
