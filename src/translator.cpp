#include "translator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

Translator::Translator(QObject *parent)
    : QTranslator(parent),
      m_languageFile(QDir::homePath() + QStringLiteral("/.config/omarchy/language"))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reload);
    reload();
}

QString Translator::translate(const char *, const char *sourceText, const char *, int) const
{
    return m_strings.value(QString::fromUtf8(sourceText), QString::fromUtf8(sourceText));
}

void Translator::reload()
{
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    const QString directory = QFileInfo(m_languageFile).absolutePath();
    if (QFileInfo::exists(directory))
        m_watcher.addPath(directory);
    if (QFileInfo::exists(m_languageFile))
        m_watcher.addPath(m_languageFile);

    QString language = QStringLiteral("en");
    QFile languageFile(m_languageFile);
    if (languageFile.open(QIODevice::ReadOnly)) {
        const QString code = QString::fromUtf8(languageFile.readLine()).trimmed();
        static const QRegularExpression valid(QStringLiteral("^[a-z]{2,3}(-[A-Za-z]{2})?$"));
        if (valid.match(code).hasMatch())
            language = code;
    }

    QHash<QString, QString> strings;
    QStringList codes{QStringLiteral("en"), language.section(QLatin1Char('-'), 0, 0), language};
    codes.removeDuplicates();
    for (const auto &code : codes) {
        QFile catalog(QStringLiteral(":/i18n/") + code + QStringLiteral(".json"));
        if (!catalog.open(QIODevice::ReadOnly))
            continue;
        const auto object = QJsonDocument::fromJson(catalog.readAll()).object();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (it.value().isString() && !it.value().toString().isEmpty())
                strings.insert(it.key(), it.value().toString());
    }

    m_language = language;
    m_strings = strings;
    const QLocale locale(language);
    QLocale::setDefault(locale);
    QGuiApplication::setLayoutDirection(locale.textDirection());
    emit changed();
}
