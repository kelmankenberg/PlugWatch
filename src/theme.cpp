#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

Theme::Theme(QObject *parent) : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
    if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
        m_hyprctl = QStandardPaths::findExecutable(QStringLiteral("hyprctl"));
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput())
                               .object().value(QStringLiteral("int"));
        if (exitCode != 0 || exitStatus != QProcess::NormalExit || !value.isDouble())
            return;
        const int radius = qMax(0, value.toInt());
        if (radius != m_radius) {
            m_radius = radius;
            emit changed();
        }
    });
    connect(&m_roundingProcess, &QProcess::errorOccurred, this,
            [this] { m_roundingTimeout.stop(); });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme()
{
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding()
{
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {QStringLiteral("-j"), QStringLiteral("getoption"),
                                        QStringLiteral("decoration:rounding")});
    m_roundingTimeout.start();
}

void Theme::reload()
{
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    const QString current = QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
    QString ancestor = current;
    while (!QFileInfo::exists(ancestor) && ancestor != QStringLiteral("/"))
        ancestor = QFileInfo(ancestor).absolutePath();
    const QStringList candidates{
        ancestor,
        QFileInfo(ancestor).absolutePath(),
        current,
        current + QStringLiteral("/theme"),
        current + QStringLiteral("/theme/colors.toml")
    };
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    QString accent = QStringLiteral("#FFD60A");
    QFile file(current + QStringLiteral("/theme/colors.toml"));
    if (file.open(QIODevice::ReadOnly)) {
        const QRegularExpression expression(
            QStringLiteral(R"re(^\s*accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])re"),
            QRegularExpression::MultilineOption);
        const auto match = expression.match(QString::fromUtf8(file.readAll()));
        if (match.hasMatch())
            accent = match.captured(1);
    }
    if (accent != m_accent) {
        m_accent = accent;
        emit changed();
    }
}
