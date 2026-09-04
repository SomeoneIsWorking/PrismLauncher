// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHostAddress>
#include <QList>

namespace Lan {

bool isPrivateIpv4(const QHostAddress& address);
QList<QHostAddress> privateLanAddresses();

}  // namespace Lan
