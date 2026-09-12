# Project state

Factual capability coverage for Prism Launcher. Durable intent is in
`docs/project-goals.md`; atomic work is in `docs/issues/`; subsystem ownership
is in `docs/codemap.md`.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | The launcher manages and launches Minecraft instances | verified | — | G001 |
| S002 | Instances can use the established archive import and export paths | verified | S001 | G001,G003 |
| S003 | Every running launcher automatically advertises its instance catalogue on the LAN | verified | S002 | G002 |
| S004 | A recipient can discover and import an available instance from the New Instance window | verified | S002,S003 | G002 |
| S005 | LAN transfer uses a bounded capability and preserves the existing importer as archive authority | verified | S002,S003,S004 | G002,G003 |
| S006 | LAN import has focused positive and negative verification | verified | S003,S004,S005 | G003 |
| S007 | Players can select, configure, and manage compatible Java runtimes | verified | S001 | G001 |
| S008 | Players can authenticate or use the launcher-supported account workflows | partial | S001 | G001 |
| S009 | Players can browse and install supported mod-platform content | partial | S001 | G001 |
| S010 | Players can configure launcher presentation, logging, and updates | partial | S001 | G001 |
| S011 | A selected instance can create a desktop shortcut using its instance name | verified | S001 | G001 |
| S012 | The fork's installed Flatpak checks and installs stable fork releases automatically | partial | S010 | G001 |
| S013 | Flatpak launcher data can be copied into a native installation | partial | S001 | G001 |
| S014 | Fork releases provide a Linux AppImage, Flatpak, and native macOS arm64 app | verified | S012 | G001 |
| S015 | A recipient can update an existing stopped local instance from a LAN instance while preserving local player data | verified | S003,S005 | G002,G003 |

## Comparison baseline

The comparison baseline is upstream Prism Launcher at this fork's imported
upstream checkpoint (`3d01e09fc`). S003-S006 add automatic local-network
instance import, while S011 changes the selected-instance desktop shortcut
workflow. The other state items describe retained upstream launcher
capabilities whose current evidence is tracked independently. S012 and S013
add fork-specific maintenance and an optional native-package migration path.
S014 tracks the fork's release packages across Linux and Apple Silicon macOS.
S015 adds LAN updates for an existing instance in addition to creating a copy.

## Current focus

S008 — Account workflow evidence.

## Capability details

### S001 — Instance management and launch

Verified. The GitHub-built 12.0.7 Flatpak selected the existing
`Supermarket-pack` instance, applied its declared LWJGL 3.3.1, Minecraft
1.20.1, intermediary, and Fabric Loader 0.19.3 components, started its
configured Java 17 runtime, and reached a rendered Minecraft 1.20.1/Fabric main
menu. Minecraft exited through its Quit action and recorded a clean
`Stopping!` event.

Evidence: the 2026-09-04 packaged release run from commit `2960f237d`, with
OpenAL initialized on the silent `No Output` device and no interactive desktop
session used.

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

Verified. The GitHub-built 12.0.7 Flatpak validated Microsoft Java 17.0.15 on
`amd64` both normally and with the configured 4 GiB heap. Its managed-runtime
workflow downloaded Mojang `java-runtime-gamma` 17.0.15 into an isolated
launcher root, discovered and displayed the installation, and removed it
through the confirmation flow. A maximum-memory change from 4096 MiB to 3072
MiB made through the Java settings page persisted to the isolated launcher
configuration.

Evidence: the 2026-09-04 Java settings and Installations workflows in packaged
release 12.0.7 from commit `2960f237d`, including successful JavaCheck output
and the resulting `MaxMemAlloc=3072` setting.

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

### S012 — Fork Flatpak updates

Partial. The installed 12.0.7 Flatpak reports `Updates Enabled: No`: its
manifest omitted `Launcher_BUILD_ARTIFACT`, and Prism's updater refused Flatpak
installation even when a check succeeded. The fork manifest now enables Prism's
existing Update Available flow, and `FlatpakUpdate` verifies a release bundle
before importing it into a local Flatpak repository and invoking the host
Flatpak updater. The prior external user timer has been disabled and removed.
The current user's installation was bound to `prism-fork-local`; a change from
the 12.0.7 release commit to the Clang-built local package was installed with
`flatpak update`, preserving the 3.1 GB launcher data. A Flatpak mask on the app
had hidden updates; it was removed after switching the origin to the fork. The
packaged launcher now logs `Updates Enabled: Yes` and its packaged updater
successfully checks the fork's GitHub release list. After release `12.0.8`, the
installed `12.0.7` Flatpak's own updater returned update-available exit status
100, selected `PrismLauncher-12.0.8-x86_64.flatpak`, downloaded and verified
it, and installed it through the host Flatpak update path. `flatpak info` then
reported `12.0.8`, origin `prism-fork-local`, commit `9d42c3c6e`, while the
launcher data directory remained 3.1 GB.

Gap: the launcher's Update Available dialog and acceptance click have not yet
been observed end-to-end; the same packaged updater was invoked directly for
this install check.

### S013 — Optional native-package migration

Partial. `tools/migrate_flatpak_data.py` offers dry-run and explicit copy modes.
Focused tests proved that it copies instance data, remaps absolute launcher
paths in `.cfg` files, preserves the Flatpak source, and refuses to overwrite
existing native data. The dry run located the current user's 3.1 GB launcher
data directory.

Gap: the full real-user migration has not been executed because the Flatpak
remains the installed target.

### S014 — Fork release packages

Verified. The tagged `12.0.8` run `34694873327` passed its AppImage, Flatpak,
native macOS arm64 build/test/package, and publish jobs. The release contains
`PrismLauncher-Linux-x86_64.AppImage` and its `.zsync` index,
`PrismLauncher-12.0.8-x86_64.flatpak`, `PrismLauncher-macOS-arm64.app.zip`,
`PrismLauncher-macOS-arm64.dmg`, and `SHA256SUMS`. CI checked the app executable's
arm64 architecture and verified the bundle signature before publication. With
no Apple certificate or notarization credentials configured, the macOS bundle
uses ad-hoc signing and is not notarized.

Evidence: GitHub Actions release run `34694873327` and the published `12.0.8`
asset list, including the arm64 architecture and signature checks.

### S015 — LAN update of an existing instance

Verified. The LAN page lets a player choose a stopped local instance and
confirm an update from a discovered sender. The existing archive importer
downloads and extracts the complete Prism instance. A dedicated update owner
preserves the local instance configuration, worlds, screenshots, resource and
shader packs, options, and server list while replacing pack files in a staging
directory. Commit renames the old instance aside and installs the staged tree,
restoring the old tree on a failed install. The 2026-09-12 Clang build and all
26 Qt tests pass, including removal of an obsolete mod, preservation of local
world data, exclusion of the sender's world, and refusal of an incomplete
archive. An isolated two-launcher GUI run updated `Local Pack` twice from
`Remote Pack`: each transfer removed the older mod, installed the next one,
kept the receiver's world, options, and name, and excluded the sender's world.
The first run also imported a separate `Remote Pack` copy through the original
action, retaining its sender world, and the second run reopened the updated
local instance before applying another update.

Evidence: the 2026-09-12 Clang SDK full build and 26-test suite, plus the
isolated two-launcher GUI runs and byte-level checks of the receiver's mods,
worlds, options, and instance configuration.
