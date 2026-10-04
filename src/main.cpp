#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QQuickStyle>
#include <QIcon>
#include <QSettings>
#include <QWindow>
#include <QLocale>
#include <memory>
#include "backend/RocksDBBackend.h"
#include "backend/EntryModel.h"
#include "backend/FilterProxyModel.h"
#include "utils/JsonUtils.h"
#include "utils/I18nManager.h"
#include "utils/FileSystemModel.h"
#include "utils/SettingsMigration.h"
#include "utils/SettingsKeys.h"
#include "utils/WindowGeometry.h"

/**
 * @brief アプリケーションのエントリーポイント
 *
 * Qt/QMLアプリケーションを初期化し、必要なシングルトン・型を登録してメインQMLを読み込む
 * コマンドライン引数でデータベースパスが指定された場合は起動時に開く
 *
 * @param argc コマンドライン引数の数
 * @param argv コマンドライン引数の配列
 * @return アプリケーションの終了コード
 */
int main(int argc, char *argv[])
{
    // QtGUIアプリケーションを初期化する
    QGuiApplication app(argc, argv);

    // QStandardPathsとQSettingsが参照するアプリケーション識別情報を設定する
    // SettingsMigration::migrateIfNeeded()は、ConfigLocationに組織名とアプリケーション名を含むパスを使用する
    // そのため、この設定より後に呼び出す
    app.setOrganizationName("Presire");
    app.setOrganizationDomain("https://github.com/presire");
    app.setApplicationName("RocksDBViewer");
    app.setApplicationVersion(ROCKSDBVIEWER_VERSION);

    SettingsMigration::migrateIfNeeded();

    QIcon appIcon;
    appIcon.addFile(":/RocksDBViewerApp/assets/RocksDBViewer@128.png", QSize(128, 128));
    appIcon.addFile(":/RocksDBViewerApp/assets/RocksDBViewer@256.png", QSize(256, 256));
    appIcon.addFile(":/RocksDBViewerApp/assets/RocksDBViewer@512.png", QSize(512, 512));
    app.setWindowIcon(appIcon);

    // Materialスタイルを設定する
    // KDEPlasmaではプラットフォームテーマによりBreezeスタイルが選択され、ツールチップのbinding loop警告やレイアウト崩れの原因になる
    // そのため環境変数ではなくAPIで明示的に指定し、QQmlApplicationEngineの生成前に呼び出す
    QQuickStyle::setStyle("Material");

    std::unique_ptr<RocksDBBackend> backend = std::make_unique<RocksDBBackend>();
    std::unique_ptr<I18nManager> i18n;
    QQmlApplicationEngine engine;
    i18n = std::make_unique<I18nManager>(&engine);

    // QMLで生成する型を登録する
    qmlRegisterType<EntryModel>("RocksDBViewerApp", 1, 0, "EntryModel");
    qmlRegisterType<FilterProxyModel>("RocksDBViewerApp", 1, 0, "FilterProxyModel");
    qmlRegisterType<FileSystemModel>("RocksDBViewerApp", 1, 0, "FileSystemModel");
    qmlRegisterSingletonType<JsonUtils>("RocksDBViewerApp", 1, 0, "JsonUtils",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new JsonUtils();
        });

    // QMLから常にアクセス可能なシングルトンを登録する
    qmlRegisterSingletonInstance("RocksDBViewerApp", 1, 0, "Backend", backend.get());
    qmlRegisterSingletonInstance("RocksDBViewerApp", 1, 0, "I18n", i18n.get());

    // 言語設定を永続化し、未設定時はシステムロケールから検出する
    auto settings = SettingsMigration::newSettings();
    QString savedLang = settings.value(SettingsKeys::language, QString()).toString();
    if (savedLang != QStringLiteral("ja") && savedLang != QStringLiteral("en")) {
        if (savedLang.isEmpty()) {
            QLocale locale = QLocale::system();
            QString lang = locale.name().left(2);
            if (lang == "ja") {
                savedLang = "ja";
            }
            else {
                savedLang = "en";
            }
        }
        else {
            // 設定ファイル内の不正な言語コードを無視し、ロケール由来の既定値を使用する
            QLocale locale = QLocale::system();
            savedLang = (locale.name().left(2) == "ja") ? "ja" : "en";
        }
    }
    // 言語変更時に設定を保存する
    QObject::connect(i18n.get(), &I18nManager::currentLanguageChanged, [i18n = i18n.get()]() {
        auto s = SettingsMigration::newSettings();
        s.setValue(SettingsKeys::language, i18n->currentLanguage());
    });

    i18n->setCurrentLanguage(savedLang);
    if (settings.value(SettingsKeys::language, QString()).toString().isEmpty()) {
        settings.setValue(SettingsKeys::language, savedLang);
    }

    // ダークモード設定を読み込み、QMLへ公開する
    bool initialDarkMode = settings.value(SettingsKeys::darkMode, false).toBool();
    engine.rootContext()->setContextProperty("initialDarkMode", initialDarkMode);

    // メインQMLを読み込む
    const QUrl url(QStringLiteral("qrc:/RocksDBViewerApp/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    // コマンドライン引数からデータベースパスを取得する
    // パスの検証はopenDatabase側で行い、エラーはerrorOccurred経由でトースト表示する
    QStringList args = app.arguments();
    if (args.size() > 1) {
        QString dbPath = args.at(1);
        backend->openDatabase(dbPath);
    }

    // Linuxタスクバーとの互換性のため、ウィンドウ状態を復元してアイコンを設定する
    const auto windows = app.topLevelWindows();
    if (!windows.isEmpty()) {
        QWindow *window = windows.first();
        window->setIcon(QIcon(":/RocksDBViewerApp/assets/RocksDBViewer@512.png"));
        if (settings.contains(SettingsKeys::windowX)) {
            // 前回と異なるモニタ構成やスケーリング設定でも画面内に収まるよう、起動のたびに補正する
            const QRect saved(settings.value(SettingsKeys::windowX).toInt(),
                              settings.value(SettingsKeys::windowY).toInt(),
                              settings.value(SettingsKeys::windowWidth, 1400).toInt(),
                              settings.value(SettingsKeys::windowHeight, 900).toInt());
            window->setGeometry(WindowGeometry::fitToScreens(saved,
                                                              WindowGeometry::availableGeometries(),
                                                              QSize(100, 100),
                                                              QSize(1400, 900)));
        }

        // 終了時にウィンドウ状態を保存する
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [window]() {
            auto s = SettingsMigration::newSettings();
            s.setValue(SettingsKeys::windowX, window->x());
            s.setValue(SettingsKeys::windowY, window->y());
            s.setValue(SettingsKeys::windowWidth, window->width());
            s.setValue(SettingsKeys::windowHeight, window->height());
        });
    }

    return app.exec();
}
