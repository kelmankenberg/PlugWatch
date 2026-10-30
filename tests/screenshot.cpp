#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    const QString output = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("plugwatch.png");
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("XDG_DATA_HOME", (home.path() + QStringLiteral("/data")).toLocal8Bit());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("plugwatch"));
    app.setDesktopFileName(QStringLiteral("plugwatch"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDirectory))
        return 1;
    QFile state(dataDirectory + QStringLiteral("/state.json"));
    if (!state.open(QIODevice::WriteOnly))
        return 1;
    state.write(R"json({
      "lastChecked": "2026-10-30T08:30:00",
      "knownIds": ["pomo", "workspace.peek", "safe-shell"],
      "newIds": ["pomo", "workspace.peek"],
      "plugins": [
        {
          "id": "pomo",
          "name": "Pomo",
          "author": "June Park",
          "category": "Productivity",
          "description": "A calm focus timer that keeps your next break in view without interrupting your work.",
          "thumbnailUrl": "https://plugins.omarchy.org/assets/img/plugins/8-jjdizz1l-dizziee.cline-model-usage-card.webp",
          "repo": "https://github.com/example/pomo",
          "installCommand": "omarchy plugin add https://github.com/example/pomo.git --enable",
          "installNote": "Install from the upstream repository.",
          "listedAt": "2026-10-30T08:05:00Z",
          "tags": ["focus", "timer"],
          "marketplaceUrl": "https://plugins.omarchy.org/plugin.html?id=pomo"
        },
        {
          "id": "workspace.peek",
          "name": "Workspace Peek",
          "author": "Kai Rivers",
          "category": "Desktop",
          "description": "Preview open workspaces at a glance and jump to a window with a single shortcut.",
          "repo": "https://github.com/example/workspace-peek",
          "installCommand": "",
          "installNote": "Follow the setup guide in the upstream repository.",
          "listedAt": "2026-10-30T07:45:00Z",
          "tags": ["workspaces", "hyprland"],
          "marketplaceUrl": "https://plugins.omarchy.org/plugin.html?id=workspace.peek"
        },
        {
          "id": "safe-shell",
          "name": "Safe Shell",
          "author": "Rae Lin",
          "category": "Security",
          "description": "Small shell helpers for reviewing commands before they run.",
          "repo": "https://github.com/example/safe-shell",
          "installCommand": "omarchy plugin add https://github.com/example/safe-shell.git --enable",
          "installNote": "Install from the upstream repository.",
          "listedAt": "2026-10-29T14:00:00Z",
          "tags": ["security", "system"],
          "marketplaceUrl": "https://plugins.omarchy.org/plugin.html?id=safe-shell"
        }
      ]
    })json");
    state.close();

    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend;
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    QTimer::singleShot(1500, &app, [window, output, &app] {
        const QImage image = window->grabWindow();
        if (image.isNull() || !image.save(output))
            app.exit(1);
        else
            app.exit(0);
    });
    return app.exec();
}
