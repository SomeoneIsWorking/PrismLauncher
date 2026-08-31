# Project state

Factual capability coverage for Prism Launcher. Durable intent is in
`docs/project-goals.md`; atomic work is in `docs/issues/`; subsystem ownership
is in `docs/codemap.md`.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | The launcher manages and launches Minecraft instances | partial | — | G001 |
| S002 | Instances can use the established archive import and export paths | partial | S001 | G001,G003 |
| S003 | A selected instance can be explicitly offered on the local network | partial | S002 | G002 |
| S004 | A recipient can find or enter an authorised LAN offer and import it | partial | S002,S003 | G002 |
| S005 | LAN transfer uses a bounded capability and preserves the existing importer as archive authority | partial | S002,S003,S004 | G002,G003 |
| S006 | LAN sharing has focused positive and negative verification | partial | S003,S004,S005 | G003 |
| S007 | Players can select, configure, and manage compatible Java runtimes | partial | S001 | G001 |
| S008 | Players can authenticate or use the launcher-supported account workflows | partial | S001 | G001 |
| S009 | Players can browse and install supported mod-platform content | partial | S001 | G001 |
| S010 | Players can configure launcher presentation, logging, and updates | partial | S001 | G001 |
| S011 | A selected instance can create a desktop shortcut using its instance name | partial | S001 | G001 |

## Current focus

S003 — Explicit local-network instance offers.

## Capability details

### S001 — Instance management and launch

Partial. The launcher contains the established instance and launch subsystems,
but this inventory has not yet rerun the complete user-facing launch workflow.

Gap: current end-to-end evidence, not a claim that source presence alone proves
the outcome.

### S002 — Established archive workflows

Partial. `InstanceImportTask` owns URL/archive processing and
`MMCZip::ExportToZipTask` owns ZIP writing.

Gap: a current combined regression proving their retained behavior once LAN
sharing is added.

### S003 — Explicit LAN offer

Partial. `Lan::ShareController` creates the instance archive and `Lan::Offer`
owns the explicit temporary capability-protected local-network offer, while
`LanShareDialog` exposes its Start and Stop actions. Active offers announce
their capability URLs over a bounded UDP LAN-discovery channel. `LanOffer`
proves a successful byte-for-byte transfer in the build environment.

Gap: the combined launcher build and live local-network transfer have not yet
verified this source implementation.

### S004 — Authorised recipient import

Partial. `Add Instance -> Import from LAN...` listens only while its dialog is
open, discovers active offers automatically, and hands the selected validated
URL to the established `processURLs`/`InstanceImportTask` path.

Gap: a live receiver import between two installed launchers has not yet
verified this source implementation.

### S005 — Bounded transfer authority

Partial. The offer creates a 256-bit capability URL, only serves its temporary
archive while the dialog remains active, and stops its Lucent server on Stop or
destruction. The focused test proves that a wrong capability receives 404.

Gap: combined transfer and rejection evidence has not yet verified the new
boundary.

### S006 — LAN-sharing verification

Partial. `LanOffer` verifies byte-exact archive transfer for a valid
capability and 404 rejection for a wrong capability. The recipient's full
`InstanceImportTask` workflow is still awaiting an installed-launcher run.

Gap: install the finished launcher and verify a recipient import through the
normal UI.

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

Partial. The selected-instance action creates a desktop shortcut directly with
the instance name; it no longer opens a name or destination dialog.

Gap: the installed launcher's desktop integration still needs verification.
