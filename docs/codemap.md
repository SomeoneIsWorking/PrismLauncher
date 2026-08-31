# Codemap

| Subsystem | Responsibility | Current / target location | Entry point |
|---|---|---|---|
| Instance import | Downloads, identifies, validates, and creates imported instances | `launcher/InstanceImportTask.*` | `InstanceImportTask::executeTask` |
| Archive export | Preserves custom instance icons and writes selected instance files to a ZIP archive | `launcher/archive/InstanceArchive.*`, `launcher/archive/ExportToZipTask.*` | `MMCZip::saveInstanceIcon`, `MMCZip::ExportToZipTask::executeTask` |
| Export UI | Collects export selection and invokes archive export | `launcher/ui/dialogs/ExportInstanceDialog.*` | `ExportInstanceDialog::doExport` |
| Main-window instance actions | Wires selected-instance commands to dialogs | `launcher/ui/MainWindow.*` | `MainWindow::on_actionExportInstanceZip_triggered` |
| Desktop shortcut action | Creates a named desktop launch entry for the selected instance | `launcher/ui/MainWindow.*`, `launcher/minecraft/ShortcutUtils.*` | `MainWindow::on_actionCreateInstanceShortcut_triggered` |
| LAN sharing | Owns offer lifecycle, authorisation, local transfer, and importer handoff | `launcher/lan/` | `Lan::ShareController`, `Lan::Offer` |
| LAN sharing UI | Presents start/stop share and recipient import actions | `launcher/ui/dialogs/` | target: LAN share dialogs |

## Where does X go?

- Archive creation goes through `MMCZip::ExportToZipTask`; LAN sharing must not
  serialize a second archive format.
- Archive validation and instance creation go through `InstanceImportTask`; a
  LAN recipient supplies it a URL rather than extracting files itself.
- Local HTTP serving belongs in Lucent, consumed by `launcher/lan/`.
- Main-window code only wires the selected action to the LAN UI; it does not
  own transfer state or protocol policy.
