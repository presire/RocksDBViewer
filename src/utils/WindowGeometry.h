#pragma once

#include <QList>
#include <QRect>
#include <QSize>

/**
 * @namespace WindowGeometry
 * @brief 保存済みウィンドウ位置・サイズを現在の画面構成に収める
 *
 * すべての値はデバイス非依存ピクセル (論理ピクセル) で扱う
 * QWindowの座標・サイズとQScreen::availableGeometry()が同じ単位のため、スケーリング設定の変更にも追従できる
 */
namespace WindowGeometry {

/**
 * @brief 現在接続されている全画面の利用可能領域を取得する
 *
 * タスクバーやパネルが占有する領域を除いた領域を返す
 * 先頭要素はプライマリスクリーンである
 *
 * @return 各画面のavailableGeometry()の一覧 (画面がない場合は空)
 */
QList<QRect> availableGeometries();

/**
 * @brief 保存済みジオメトリを現在の画面構成内に収めた矩形へ補正する
 *
 * 補正規則は以下のとおり
 * - 幅または高さが[minimumSize]未満の場合は[defaultSize]の該当寸法に置き換える
 * - 保存矩形と重なる面積が最大の画面を配置先とする
 * - どの画面とも重ならない場合(モニタ構成の変更など)はプライマリスクリーンの中央へ配置する
 * - サイズは配置先画面の利用可能領域を超えないよう制限する
 * - 位置は矩形全体が配置先画面の利用可能領域に収まるよう制限する
 *
 * @param saved 保存済みのウィンドウ矩形
 * @param screens 各画面の利用可能領域 (先頭をプライマリスクリーンとする)
 * @param minimumSize 有効とみなす最小サイズ
 * @param defaultSize 無効なサイズの代わりに使用する既定サイズ
 * @return 画面内に収まるよう補正した矩形 (screensが空の場合はサイズのみ補正した矩形)
 */
QRect fitToScreens(const QRect &saved,
                   const QList<QRect> &screens,
                   const QSize &minimumSize,
                   const QSize &defaultSize);

}
