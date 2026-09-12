# Codemap

| Subsystem | Responsibility | Current / target location | Entry point |
|---|---|---|---|
| Instance import | Downloads, identifies, validates, and creates imported instances | `launcher/InstanceImportTask.*` | `InstanceImportTask::executeTask` |
| Archive export | Preserves custom instance icons and writes selected instance files to a ZIP archive | `launcher/archive/InstanceArchive.*`, `launcher/archive/ExportToZipTask.*` | `MMCZip::saveInstanceIcon`, `MMCZip::ExportToZipTask::executeTask` |
| Export UI | Collects export selection and invokes archive export | `launcher/ui/dialogs/ExportInstanceDialog.*` | `ExportInstanceDialog::doExport` |
| Main-window instance actions | Wires selected-instance commands to dialogs | `launcher/ui/MainWindow.*` | `MainWindow::on_actionExportInstanceZip_triggered` |
| Desktop shortcut action | Creates a named desktop launch entry for the selected instance | `launcher/ui/MainWindow.*`, `launcher/minecraft/ShortcutUtils.*` | `MainWindow::on_actionCreateInstanceShortcut_triggered` |
| LAN discovery protocol | Owns private-interface selection and strict bounded catalogue/request messages | `launcher/lan/LanNetwork.*`, `launcher/lan/LanProtocol.*` | `Lan::privateIPv4Addresses`, `Lan::parseDatagram` |
| LAN instance service | Owns automatic catalogue advertisement, remote expiry, on-demand archive preparation, capability offers, cancellation, and timeouts | `launcher/lan/LanInstanceService.*`, `launcher/lan/LanOffer.*` | `Lan::InstanceService::start`, `Lan::InstanceService::requestImport` |
| LAN import UI | Presents discovered instances inside New Instance and hands a prepared URL to the established importer | `launcher/ui/pages/modplatform/LanPage.*`, `launcher/ui/dialogs/NewInstanceDialog.*` | `LanPage::prepareSelected`, `NewInstanceDialog::importFromLan` |
| LAN existing-instance update | Selects a stopped local target, stages a complete Prism archive through the importer, preserves local player data, and swaps the instance directory with rollback | `launcher/ui/pages/modplatform/LanPage.*`, `launcher/InstanceImportTask.*`, `launcher/lan/LanUpdate.*`, `launcher/InstanceList.*` | `LanPage::updateSelected`, `Lan::prepareInstanceUpdate`, `Lan::commitInstanceUpdate` |
| Fork Flatpak updates | Selects and verifies fork release bundles, imports them into the local repository, and invokes the host Flatpak updater from Prism's existing update flow | `launcher/updater/prismupdater/FlatpakUpdate.*`, `launcher/updater/prismupdater/PrismUpdater.*`, `tools/setup_fork_flatpak.py` | `FlatpakUpdate::bundleProblem`, `FlatpakUpdate::installBundle` |
| Native data migration | Copies the Flatpak launcher data to a native data root without overwriting either installation | `tools/migrate_flatpak_data.py` | `migrate` |
| Fork release packaging | Builds and publishes AppImage, Flatpak, and macOS arm64 app artifacts from one tag | `.github/workflows/release.yml`, `.github/actions/package/{linux,macos}/action.yml` | `publish` job |

## Where does X go?

- Archive creation goes through `MMCZip::ExportToZipTask`; LAN sharing must not
  serialize a second archive format.
- Archive validation and instance creation go through `InstanceImportTask`; a
  LAN recipient supplies it a URL rather than extracting files itself.
- A LAN update also uses `InstanceImportTask` for archive extraction, then
  `LanUpdate` for local-data policy and directory replacement.
- Local HTTP serving belongs in Lucent, consumed by `launcher/lan/`.
- The application owns one long-lived `Lan::InstanceService`; main-window code
  does not own LAN lifecycle or protocol policy.
