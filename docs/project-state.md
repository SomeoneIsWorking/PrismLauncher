# Project state

Factual capability coverage for Prism Launcher. Durable intent is in
`docs/project-goals.md`; atomic work is in `docs/issues/`; subsystem ownership
is in `docs/codemap.md`.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | The launcher manages and launches Minecraft instances | partial | — | G001 |
| S002 | Instances can use the established archive import and export paths | verified | S001 | G001,G003 |
| S003 | Every running launcher automatically advertises its instance catalogue on the LAN | verified | S002 | G002 |
| S004 | A recipient can discover and import an available instance from the New Instance window | verified | S002,S003 | G002 |
| S005 | LAN transfer uses a bounded capability and preserves the existing importer as archive authority | verified | S002,S003,S004 | G002,G003 |
| S006 | LAN import has focused positive and negative verification | verified | S003,S004,S005 | G003 |
| S007 | Players can select, configure, and manage compatible Java runtimes | partial | S001 | G001 |
| S008 | Players can authenticate or use the launcher-supported account workflows | partial | S001 | G001 |
| S009 | Players can browse and install supported mod-platform content | partial | S001 | G001 |
| S010 | Players can configure launcher presentation, logging, and updates | partial | S001 | G001 |
| S011 | A selected instance can create a desktop shortcut using its instance name | verified | S001 | G001 |

## Comparison baseline

The comparison baseline is upstream Prism Launcher at this fork's imported
upstream checkpoint (`3d01e09fc`). S003-S006 add automatic local-network
instance import, while S011 changes the selected-instance desktop shortcut
workflow. The other state items describe retained upstream launcher
capabilities whose current evidence is tracked independently.

## Current focus

S001 — Current end-to-end instance management and launch evidence.

## Capability details

### S001 — Instance management and launch

Partial. The launcher contains the established instance and launch subsystems,
but this inventory has not yet rerun the complete user-facing launch workflow.

Gap: current end-to-end evidence, not a claim that source presence alone proves
the outcome.

### S002 — Established archive workflows

Verified. `InstanceImportTask` remains the sole URL/archive importer and
`MMCZip::ExportToZipTask` remains the ZIP writer. A two-launcher run on
2026-09-04 exported an instance on demand through the LAN service, imported it
through `InstanceImportTask`, preserved its name, and reproduced a marker file
byte-for-byte in the receiver's instance directory.

Evidence: Clang `LanOffer`, `LanProtocol`, and `LanInstanceService` tests plus
the 2026-09-04 two-launcher UI import.

### S003 — Automatic LAN catalogue

Verified. Application-owned `Lan::InstanceService` starts after the instance
list loads, advertises every instance over the bounded LAN protocol, marks live
instances unavailable, refreshes the catalogue periodically, and expires stale
remote entries. Discovery and request traffic use separate fixed-discovery and
ephemeral-control sockets so multiple launchers on one host do not misroute a
request.

Evidence: Clang `LanProtocol` and `LanInstanceService` tests plus automatic
discovery in the 2026-09-04 two-launcher UI run.

### S004 — Recipient LAN import

Verified. The New Instance window contains an `Import from LAN` page directly
below `Import`. It lists automatically discovered instances, prevents selection
of a running instance, requests preparation on demand, preserves the advertised
instance name, and hands the capability URL to the established Import page. A
two-launcher UI run on 2026-09-04 completed the import and displayed the new
instance in the receiver.

Evidence: 2026-09-04 two-launcher UI import of `Family LAN Pack`, including its
name and marker file in the receiver.

### S005 — Bounded transfer authority

Verified. The sender creates a fresh 256-bit capability for one temporary
archive, serves it with Lucent, rejects incorrect capabilities, and expires the
offer automatically. The protocol reconstructs the HTTP host from the observed
UDP sender rather than trusting a network-supplied host. Cancellation, timeout,
and a source instance becoming live all terminate preparation without
publishing a partial archive. The receiver delegates archive validation and
creation to `InstanceImportTask`.

Evidence: Clang `LanOffer`, `LanProtocol`, and `LanInstanceService` tests plus
the 2026-09-04 end-to-end import.

### S006 — LAN-import verification

Verified. `LanOffer` covers byte-exact transfer and wrong-capability rejection;
`LanProtocol` covers valid round trips plus malformed, oversized, wrong-type,
public-address, loopback, and unsafe-URL rejection; `LanInstanceService` covers
catalogue discovery, ephemeral reply routing, request, cancellation, and ready
handoff. All three focused tests pass in the Clang SDK build, and the 2026-09-04
two-launcher UI run covers the shipping discovery-to-import path.

Evidence: passing Clang `ctest -R
'^(LanOffer|LanProtocol|LanInstanceService)$'` and the 2026-09-04 two-launcher
UI run.

### S007 — Java runtime management

Partial. The `launcher/java/` and `libraries/javacheck/` subsystems provide the
current implementation boundary.

Gap: current user-facing verification has not been recorded in this inventory.

### S008 — Account workflows

Partial. The `launcher/minecraft/auth/` subsystem owns launcher-supported
account flows.

Gap: current user-facing verification has not been recorded in this inventory.

### S009 — Mod-platform workflows

Partial. The `launcher/modplatform/` subsystem owns supported content discovery
and installation paths.

Gap: current user-facing verification has not been recorded in this inventory.

### S010 — Launcher configuration and maintenance

Partial. The settings, logging, presentation, and updater subsystems own these
launcher capabilities. The Flatpak manifest stages the pinned Lucent source
outside the network-isolated build sandbox; a local Release build installed
version 12.0.4 to the user's `org.prismlauncher.PrismLauncher` Flatpak
deployment on 2026-08-31.

Gap: the user-facing updater workflow has not been verified.

### S011 — Direct desktop shortcut

Verified. The selected-instance action creates a desktop shortcut directly
with the instance name; it no longer opens a name or destination dialog. The
Flatpak grants access to the standard desktop directory used by the existing
cross-platform shortcut writer.

Evidence: a packaged Flatpak run on 2026-09-04 created and registered
`Codex Shortcut Verification.desktop` with the expected Flatpak launch command
in one click and displayed only the completion message. The negative control
proved that the prior manifest denied the same desktop write.
