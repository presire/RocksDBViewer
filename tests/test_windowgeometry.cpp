#include <QtTest/QtTest>

#include "utils/WindowGeometry.h"

/**
 * @class TestWindowGeometry
 * @brief WindowGeometry::fitToScreens()の補正規則を検証する
 */
class TestWindowGeometry : public QObject
{
    Q_OBJECT

private:
    const QSize m_minimum{100, 100};
    const QSize m_default{1400, 900};
    const QList<QRect> m_single{QRect(0, 0, 1920, 1080)};
    // 左にサブモニタ、右にプライマリ(先頭がプライマリ)
    const QList<QRect> m_dual{QRect(0, 0, 1920, 1080), QRect(-1280, 0, 1280, 1024)};

private slots:
    /**
     * @brief 画面内に収まる矩形は変更されない
     */
    void keepsValidRect()
    {
        const QRect saved(100, 80, 1400, 900);
        QCOMPARE(WindowGeometry::fitToScreens(saved, m_single, m_minimum, m_default), saved);
    }

    /**
     * @brief 画面外(右下)に保存された位置は画面内へ戻される
     */
    void clampsOffscreenPosition()
    {
        const QRect saved(1800, 1000, 1400, 900);
        const QRect result = WindowGeometry::fitToScreens(saved, m_single, m_minimum, m_default);
        QVERIFY(m_single.first().contains(result));
        QCOMPARE(result.size(), saved.size());
        QCOMPARE(result, QRect(520, 180, 1400, 900));
    }

    /**
     * @brief どの画面とも重ならない場合はプライマリスクリーン中央へ配置する
     */
    void centersWhenNoScreenMatches()
    {
        const QRect saved(5000, 4000, 1400, 900);
        const QRect result = WindowGeometry::fitToScreens(saved, m_single, m_minimum, m_default);
        QCOMPARE(result, QRect(260, 90, 1400, 900));
    }

    /**
     * @brief 画面より大きいサイズは画面サイズへ制限される(高DPIスケーリング変更後など)
     */
    void clampsOversizedWindow()
    {
        const QRect saved(0, 0, 3840, 2160);
        const QRect result = WindowGeometry::fitToScreens(saved, m_single, m_minimum, m_default);
        QCOMPARE(result, m_single.first());
    }

    /**
     * @brief 無効なサイズは既定サイズへ置き換えられる
     */
    void replacesInvalidSize()
    {
        const QRect saved(10, 10, 20, 5000);
        const QRect result = WindowGeometry::fitToScreens(saved, m_single, m_minimum, m_default);
        QCOMPARE(result.width(), 1400);
        QCOMPARE(result.height(), 1080);
    }

    /**
     * @brief マルチモニタで取り外された画面上の位置はプライマリへ戻される
     */
    void recoversFromRemovedMonitor()
    {
        const QRect saved(-1200, 50, 1000, 800);
        const QRect result = WindowGeometry::fitToScreens(saved, m_single, m_minimum, m_default);
        QVERIFY(m_single.first().contains(result));
    }

    /**
     * @brief サブモニタ(負座標)上の矩形は、その画面が存在する限り維持される
     */
    void keepsRectOnSecondaryMonitor()
    {
        const QRect saved(-1200, 50, 1000, 800);
        QCOMPARE(WindowGeometry::fitToScreens(saved, m_dual, m_minimum, m_default), saved);
    }

    /**
     * @brief 重なりが最大の画面へ収められる
     */
    void picksScreenWithLargestOverlap()
    {
        const QRect saved(-200, 50, 1000, 800);
        const QRect result = WindowGeometry::fitToScreens(saved, m_dual, m_minimum, m_default);
        QVERIFY(m_dual.at(0).contains(result));
    }

    /**
     * @brief 画面情報がない場合はサイズのみ補正する
     */
    void handlesEmptyScreenList()
    {
        const QRect result = WindowGeometry::fitToScreens(QRect(5, 6, 10, 10), {}, m_minimum, m_default);
        QCOMPARE(result, QRect(5, 6, 1400, 900));
    }
};

QTEST_MAIN(TestWindowGeometry)
#include "test_windowgeometry.moc"
