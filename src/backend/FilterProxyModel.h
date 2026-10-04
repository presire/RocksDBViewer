#pragma once

#include <QSortFilterProxyModel>

/**
 * @class FilterProxyModel
 * @brief EntryModelの検索フィルタとソートを担当するプロキシモデル
 *
 * フィルタ文字列がキーまたは値に含まれる行だけを表示する (大文字小文字は区別しない)
 * ソートはキーの辞書順で行い、ソート順状態 (0:なし、1:昇順、2:降順) で切り替える
 * 表示行数が変化した場合は、countChangedシグナルでQMLへ通知する
 *
 * m_filterText : 現在のフィルタ文字列
 * m_sortOrderState : 現在のソート順状態
 */
class FilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int sortOrderState READ sortOrderState WRITE setSortOrderState NOTIFY sortOrderStateChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /**
     * @brief FilterProxyModelを構築する
     * @param parent 親QObject
     */
    explicit FilterProxyModel(QObject *parent = nullptr);

    /**
     * @brief 現在のフィルタ文字列を返す
     * @return フィルタ文字列
     */
    QString filterText() const;

    /**
     * @brief フィルタ文字列を設定する
     * @param text フィルタ文字列
     * @note 空文字列ではフィルタを解除する
     */
    void setFilterText(const QString &text);

    /**
     * @brief 現在のソート順状態を返す
     * @return ソート順状態(0:ソートなし、1:昇順、2:降順)
     */
    int sortOrderState() const;

    /**
     * @brief ソート順状態を設定する
     * @param order ソート順状態 (0:ソート解除、1:昇順、2:降順)
     * @note 0を指定するとソース側の元の並び順に戻る
     */
    void setSortOrderState(int order);

    /**
     * @brief 現在の表示行数を返す
     * @return 表示中の行数
     */
    int count() const;

signals:
    /**
     * @brief フィルタ文字列の変更を通知する
     */
    void filterTextChanged();

    /**
     * @brief ソート順状態の変更を通知する
     */
    void sortOrderStateChanged();

    /**
     * @brief 表示行数の変更を通知する
     */
    void countChanged();

protected:
    /**
     * @brief 指定行がフィルタ条件に合致するか判定する
     * @param sourceRow ソースモデルの行番号
     * @param sourceParent ソースモデルの親インデックス
     * @return キーまたは値にフィルタ文字列が含まれる場合はtrue
     * @note フィルタ文字列が空の場合は常にtrueを返す
     */
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    /**
     * @brief 2つのソースインデックスをキーで比較する
     * @param source_left 左側のソースインデックス
     * @param source_right 右側のソースインデックス
     * @return 左側のキーが右側のキーより辞書順で前の場合はtrue
     */
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    // フィルタ
    QString m_filterText;       // 現在のフィルタ文字列

    // ソート
    int m_sortOrderState = 0;   // ソート順状態(0:なし、1:昇順、2:降順)
};
