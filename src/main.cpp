#include <QGuiApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    bool checkOnly = false;
    bool setCheckTimeOnly = false;
    for (int i = 1; i < argc; ++i)
        if (QByteArray(argv[i]) == "--check")
            checkOnly = true;
        else if (QByteArray(argv[i]) == "--set-check-time")
            setCheckTimeOnly = true;
    if (checkOnly || setCheckTimeOnly)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("plugwatch"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setDesktopFileName(QStringLiteral("plugwatch"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("plugwatch")));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption pluginOption(
        QStringLiteral("plugin"),
        QStringLiteral("Select a plugin by marketplace ID when opening the full app."),
        QStringLiteral("id"));
    parser.addOption(pluginOption);
    const QCommandLineOption newOption(
        QStringLiteral("new"),
        QStringLiteral("Show only new, unseen marketplace listings."));
    parser.addOption(newOption);
    const QCommandLineOption checkOption(
        QStringLiteral("check"),
        QStringLiteral("Fetch and cache the marketplace catalog without opening a window."));
    parser.addOption(checkOption);
    const QCommandLineOption setCheckTimeOption(
        QStringLiteral("set-check-time"),
        QStringLiteral("Set the daily marketplace check time (24-hour HH:MM)."),
        QStringLiteral("time"));
    parser.addOption(setCheckTimeOption);
    parser.process(app);
    checkOnly = parser.isSet(checkOption);
    setCheckTimeOnly = parser.isSet(setCheckTimeOption);

    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend;
    if (setCheckTimeOnly) {
        if (!backend.setCheckTime(parser.value(setCheckTimeOption))) {
            qCritical().noquote() << backend.error();
            return 1;
        }
        qInfo().noquote() << backend.notice();
        return 0;
    }
    if (checkOnly) {
        QObject::connect(&backend, &Backend::checkFinished, &app, [&app](bool success, const QString &message) {
            if (success)
                qInfo().noquote() << message;
            else
                qCritical().noquote() << message;
            app.exit(success ? 0 : 1);
        });
        QTimer::singleShot(0, &backend, &Backend::refresh);
        return app.exec();
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("initialPluginId"), parser.value(pluginOption));
    engine.rootContext()->setContextProperty(QStringLiteral("initialNewOnly"), parser.isSet(newOption));
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    QTimer::singleShot(0, &backend, &Backend::refresh);
    return app.exec();
}
