#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QIcon>
#include <QSettings>
#include <QWindow>
#include <QFile>
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
    // Qt GUIアプリケーションの初期化
    QGuiApplication app(argc, argv);

    SettingsMigration::migrateIfNeeded();

    // アプリケーション識別情報の設定 (QStandardPaths / QSettings等が参照)
    app.setOrganizationName("Presire");
    app.setOrganizationDomain("https://github.com/presire");
    app.setApplicationName("RocksDBViewer");
    app.setApplicationVersion(ROCKSDBVIEWER_VERSION);

    QIcon appIcon;
    appIcon.addFile(":/RocksDBViewerApp/assets/RocksDBViewer@128.png", QSize(128, 128));
    appIcon.addFile(":/RocksDBViewerApp/assets/RocksDBViewer@256.png", QSize(256, 256));
    appIcon.addFile(":/RocksDBViewerApp/assets/RocksDBViewer@512.png", QSize(512, 512));
    app.setWindowIcon(appIcon);

    // Materialスタイルの設定
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");

    std::unique_ptr<RocksDBBackend> backend = std::make_unique<RocksDBBackend>();
    std::unique_ptr<I18nManager> i18n;
    QQmlApplicationEngine engine;
    i18n = std::make_unique<I18nManager>(&engine);

    // 型の登録
    qmlRegisterType<EntryModel>("RocksDBViewerApp", 1, 0, "EntryModel");
    qmlRegisterType<FilterProxyModel>("RocksDBViewerApp", 1, 0, "FilterProxyModel");
    qmlRegisterType<FileSystemModel>("RocksDBViewerApp", 1, 0, "FileSystemModel");
    qmlRegisterSingletonType<JsonUtils>("RocksDBViewerApp", 1, 0, "JsonUtils",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new JsonUtils();
        });

    // シングルトンの登録 (QMLから常にアクセス可能にする)
    qmlRegisterSingletonInstance("RocksDBViewerApp", 1, 0, "Backend", backend.get());
    qmlRegisterSingletonInstance("RocksDBViewerApp", 1, 0, "I18n", i18n.get());

    // 言語設定の永続化と自動検出
    auto settings = SettingsMigration::newSettings();
    QString savedLang = settings.value(SettingsKeys::language, QString()).toString();
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
    // 言語変更時に保存
    QObject::connect(i18n.get(), &I18nManager::currentLanguageChanged, [i18n = i18n.get()]() {
        auto s = SettingsMigration::newSettings();
        s.setValue(SettingsKeys::language, i18n->currentLanguage());
    });

    i18n->setCurrentLanguage(savedLang);
    if (settings.value(SettingsKeys::language, QString()).toString().isEmpty()) {
        settings.setValue(SettingsKeys::language, savedLang);
    }

    // ダークモード設定の読み込みとQMLへの公開
    bool initialDarkMode = settings.value(SettingsKeys::darkMode, false).toBool();
    engine.rootContext()->setContextProperty("initialDarkMode", initialDarkMode);

    // メインQMLの読み込み
    const QUrl url(QStringLiteral("qrc:/RocksDBViewerApp/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    // コマンドライン引数からDBパスを取得
    QStringList args = app.arguments();
    if (args.size() > 1) {
        QString dbPath = args.at(1);
        if (QFile::exists(dbPath)) {
            backend->openDatabase(dbPath);
        }
    }

    // ウィンドウ状態の復元とアイコン設定 (Linuxタスクバー互換性のため)
    const auto windows = app.topLevelWindows();
    if (!windows.isEmpty()) {
        QWindow *window = windows.first();
        window->setIcon(QIcon(":/RocksDBViewerApp/assets/RocksDBViewer@512.png"));
        if (settings.contains(SettingsKeys::windowX)) {
            window->setX(settings.value(SettingsKeys::windowX).toInt());
            window->setY(settings.value(SettingsKeys::windowY).toInt());
            window->setWidth(settings.value(SettingsKeys::windowWidth, 1400).toInt());
            window->setHeight(settings.value(SettingsKeys::windowHeight, 900).toInt());
        }

        // 終了時にウィンドウ状態を保存
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
