// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

namespace Lan {

// Prepare a downloaded full-instance archive as a replacement for a local
// instance. Pack-owned files come from the archive; player data and launcher
// settings come from the existing instance.
bool prepareInstanceUpdate(const QString& existingRoot, const QString& stagedRoot, QString* error);

// Both paths must be children of the same instances directory. A failed
// replacement restores the original directory before returning.
bool commitInstanceUpdate(const QString& existingRoot, const QString& stagedRoot, QString* error);

}  // namespace Lan
