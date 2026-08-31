// SPDX-License-Identifier: GPL-3.0-only
#pragma once

class BaseInstance;

namespace MMCZip {

// Copies a custom instance icon into the instance root before an archive is
// collected. Both manual export and LAN sharing use this same preservation
// boundary; built-in icons intentionally require no copied file.
void saveInstanceIcon(BaseInstance* instance);

}  // namespace MMCZip
