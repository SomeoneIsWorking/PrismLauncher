// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanNetwork.h"

#include <QAbstractSocket>
#include <QNetworkInterface>

namespace Lan {

bool isPrivateIpv4(const QHostAddress& address)
{
    return address.protocol() == QAbstractSocket::IPv4Protocol &&
           (address.isInSubnet(QHostAddress("10.0.0.0"), 8) || address.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
            address.isInSubnet(QHostAddress("192.168.0.0"), 16) || address.isInSubnet(QHostAddress("169.254.0.0"), 16));
}

QList<QHostAddress> privateLanAddresses()
{
    QList<QHostAddress> addresses;
    for (const auto& interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto& entry : interface.addressEntries()) {
            const auto address = entry.ip();
            if (isPrivateIpv4(address) && !addresses.contains(address)) {
                addresses.append(address);
            }
        }
    }
    return addresses;
}

}  // namespace Lan
