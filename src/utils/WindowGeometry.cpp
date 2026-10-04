#include "WindowGeometry.h"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

namespace {

/**
 * @brief 2つの矩形が重なる面積を計算する
 * @param a 矩形A
 * @param b 矩形B
 * @return 重なる面積 (重ならない場合は0)
 */
qint64 intersectionArea(const QRect &a, const QRect &b)
{
    const QRect overlap = a.intersected(b);
    if (overlap.isEmpty()) {
        return 0;
    }
    return static_cast<qint64>(overlap.width()) * overlap.height();
}

}

QList<QRect> WindowGeometry::availableGeometries()
{
    QList<QRect> result;
    const auto screens = QGuiApplication::screens();
    result.reserve(screens.size());
    for (const QScreen *screen : screens) {
        result.append(screen->availableGeometry());
    }
    return result;
}

QRect WindowGeometry::fitToScreens(const QRect &saved,
                                   const QList<QRect> &screens,
                                   const QSize &minimumSize,
                                   const QSize &defaultSize)
{
    // 幅と高さを個別に検証し、無効な寸法のみ既定値へ置き換える
    int width = saved.width() < minimumSize.width() ? defaultSize.width() : saved.width();
    int height = saved.height() < minimumSize.height() ? defaultSize.height() : saved.height();

    if (screens.isEmpty()) {
        return QRect(saved.topLeft(), QSize(width, height));
    }

    // 保存矩形との重なりが最大の画面を配置先とする
    const QRect savedRect(saved.topLeft(), QSize(width, height));
    const QRect *target = &screens.first();
    qint64 bestArea = 0;
    for (const QRect &screen : screens) {
        const qint64 area = intersectionArea(savedRect, screen);
        if (area > bestArea) {
            bestArea = area;
            target = &screen;
        }
    }

    // サイズが配置先画面を超えないよう制限する
    width = std::min(width, target->width());
    height = std::min(height, target->height());

    // どの画面とも重ならない場合はプライマリスクリーンの中央へ配置する
    if (bestArea == 0) {
        return QRect(target->x() + (target->width() - width) / 2,
                     target->y() + (target->height() - height) / 2,
                     width, height);
    }

    // 矩形全体が配置先画面に収まるよう位置を制限する
    const int x = std::clamp(saved.x(), target->left(), target->left() + target->width() - width);
    const int y = std::clamp(saved.y(), target->top(), target->top() + target->height() - height);
    return QRect(x, y, width, height);
}
