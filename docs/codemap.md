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

## Where does X go?

- Archive creation goes through `MMCZip::ExportToZipTask`; LAN sharing must not
  serialize a second archive format.
- Archive validation and instance creation go through `InstanceImportTask`; a
  LAN recipient supplies it a URL rather than extracting files itself.
- Local HTTP serving belongs in Lucent, consumed by `launcher/lan/`.
- The application owns one long-lived `Lan::InstanceService`; main-window code
  does not own LAN lifecycle or protocol policy.
