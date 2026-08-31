// SPDX-License-Identifier: GPL-3.0-only

#include "archive/InstanceArchive.h"

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "icons/IconList.h"

#include <QFileInfo>
#include <QSize>

namespace MMCZip {

void saveInstanceIcon(BaseInstance* instance)
{
    const auto iconKey = instance->iconKey();
    const auto* const icon = APPLICATION->icons()->icon(iconKey);
    if (!icon || icon->isBuiltIn()) {
        return;
    }
    const auto path = icon->getFilePath();
    if (!path.isNull()) {
        const QFileInfo info(path);
        FS::copy(path, FS::PathCombine(instance->instanceRoot(), info.fileName()))();
        return;
    }

    const auto& image = icon->m_images[icon->type()];  // Icon type is an internal fixed-array index.
    const auto sizes = image.icon.availableSizes();
    if (sizes.isEmpty()) {
        return;
    }
    QSize largest = sizes.front();
    for (const auto& size : sizes) {
        if (largest.width() * largest.height() < size.width() * size.height()) {
            largest = size;
        }
    }
    image.icon.pixmap(largest).save(FS::PathCombine(instance->instanceRoot(), iconKey + ".png"));
}

}  // namespace MMCZip
