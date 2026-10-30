#include "backend.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QGuiApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileInfo>
#include <QProcess>

namespace {
const QUrl catalogUrl(QStringLiteral("https://plugins.omarchy.org/catalog.json"));

QStringList sortedUnique(const QStringList &values)
{
    auto result = values;
    result.removeDuplicates();
    result.sort(Qt::CaseInsensitive);
    return result;
}

bool runSystemctl(const QStringList &arguments, QString *error)
{
    QProcess process;
    process.start(QStringLiteral("systemctl"), arguments);
    if (!process.waitForStarted(5000)) {
        if (error)
            *error = process.errorString();
        return false;
    }
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished();
        if (error)
            *error = QObject::tr("systemctl did not finish in time.");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString output = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *error = output.isEmpty() ? QObject::tr("systemctl failed.") : output;
        }
        return false;
    }
    return true;
}
}

Backend::Backend(QObject *parent)
    : QObject(parent),
      m_statePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/state.json"),
      m_widgetStatePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/widget.json")
{
    loadState();
    QDir().mkpath(QFileInfo(m_widgetStatePath).absolutePath());
    loadWidgetState();
    watchWidgetState();
}

bool Backend::isValidCheckTime(const QString &time)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^(?:[01]\d|2[0-3]):[0-5]\d\z)"));
    return pattern.match(time).hasMatch();
}

bool Backend::setCheckTime(const QString &time)
{
    if (!isValidCheckTime(time)) {
        setError(tr("Enter a valid time in 24-hour HH:MM format."));
        return false;
    }
    if (time == m_checkTime)
        return true;

    const QString configDirectory =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/systemd/user/plugwatch-check.timer.d");
    if (!QDir().mkpath(configDirectory)) {
        setError(tr("Could not create the systemd timer configuration directory."));
        return false;
    }
    const QString overridePath = configDirectory + QStringLiteral("/50-plugwatch.conf");
    QFile previousFile(overridePath);
    const bool hadPreviousOverride = previousFile.exists();
    QByteArray previousOverride;
    if (hadPreviousOverride) {
        if (!previousFile.open(QIODevice::ReadOnly)) {
            setError(tr("Could not read the existing timer configuration: %1")
                         .arg(previousFile.errorString()));
            return false;
        }
        previousOverride = previousFile.readAll();
    }

    auto writeOverride = [&overridePath](const QByteArray &contents) {
        QSaveFile file(overridePath);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(contents) == contents.size() && file.commit();
    };
    auto restoreOverride = [&] {
        if (hadPreviousOverride)
            return writeOverride(previousOverride);
        return !QFile::remove(overridePath) || !QFileInfo::exists(overridePath);
    };
    auto reloadTimer = [](QString *error) {
        if (!runSystemctl({QStringLiteral("--user"), QStringLiteral("daemon-reload")}, error))
            return false;
        return runSystemctl({QStringLiteral("--user"), QStringLiteral("restart"),
                             QStringLiteral("plugwatch-check.timer")}, error);
    };

    const QByteArray overrideContents =
        QStringLiteral("[Timer]\nOnCalendar=\nOnCalendar=*-*-* %1:00\n").arg(time).toUtf8();
    if (!writeOverride(overrideContents)) {
        setError(tr("Could not save the timer configuration."));
        return false;
    }

    QString timerError;
    if (!reloadTimer(&timerError)) {
        const bool restored = restoreOverride();
        QString rollbackError;
        if (restored)
            reloadTimer(&rollbackError);
        setError(tr("Could not update the daily check timer: %1").arg(timerError)
                 + (restored && rollbackError.isEmpty()
                        ? QString()
                        : tr(" The previous timer configuration could not be fully restored.")));
        return false;
    }

    const QString previousTime = m_checkTime;
    m_checkTime = time;
    QString saveError;
    if (!saveWidgetCheckTime(&saveError)) {
        m_checkTime = previousTime;
        const bool restored = restoreOverride();
        QString rollbackError;
        if (restored)
            reloadTimer(&rollbackError);
        setError(tr("The timer changed, but its shared setting could not be saved: %1")
                     .arg(saveError)
                 + (restored && rollbackError.isEmpty()
                        ? QString()
                        : tr(" The previous timer configuration could not be fully restored.")));
        return false;
    }

    watchWidgetState();
    emit checkTimeChanged();
    setError({});
    setNotice(tr("Daily check time set to %1.").arg(time));
    return true;
}

QString Backend::marketplaceUrl(const QString &pluginId)
{
    QUrl url(QStringLiteral("https://plugins.omarchy.org/plugin.html"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), pluginId);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QVariantList Backend::parseCatalog(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QObject::tr("The marketplace returned invalid catalog data.");
        return {};
    }

    const auto items = document.object().value(QStringLiteral("plugins")).toArray();
    if (items.isEmpty()) {
        if (error)
            *error = QObject::tr("The marketplace catalog did not contain any plugins.");
        return {};
    }

    QVariantList plugins;
    QStringList categories;
    for (const auto &value : items) {
        if (!value.isObject())
            continue;
        const auto source = value.toObject();
        if (source.value(QStringLiteral("sourceType")).toString() == QLatin1String("builtin"))
            continue;

        const QString id = source.value(QStringLiteral("id")).toString().trimmed();
        const QString name = source.value(QStringLiteral("name")).toString().trimmed();
        if (id.isEmpty() || name.isEmpty())
            continue;

        const QString category = source.value(QStringLiteral("category")).toString().trimmed();
        if (!category.isEmpty())
            categories.append(category);
        const QUrl repository(source.value(QStringLiteral("repo")).toString());
        const QString repositoryUrl = repository.scheme() == QLatin1String("https")
                ? repository.toString() : QString();
        const QString thumbnailPath = source.value(QStringLiteral("previewThumbnail")).toString();
        static const QRegularExpression validThumbnail(
            QStringLiteral(R"(^assets/img/plugins/[A-Za-z0-9._/-]+\.(?:webp|png|jpe?g|avif)$)"),
            QRegularExpression::CaseInsensitiveOption);
        const bool hasThumbnail = validThumbnail.match(thumbnailPath).hasMatch()
                && !thumbnailPath.contains(QStringLiteral(".."));
        const QString thumbnailUrl = hasThumbnail
                ? QUrl(QStringLiteral("https://plugins.omarchy.org/") + thumbnailPath)
                      .toString(QUrl::FullyEncoded)
                : QString();
        const bool installAvailable = source.value(QStringLiteral("installAvailable")).toBool();
        const QString command = installAvailable
                ? source.value(QStringLiteral("installCommand")).toString().trimmed() : QString();

        QVariantMap plugin{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("author"), source.value(QStringLiteral("author")).toString()},
            {QStringLiteral("category"), category},
            {QStringLiteral("description"), source.value(QStringLiteral("description")).toString()},
            {QStringLiteral("thumbnailUrl"), thumbnailUrl},
            {QStringLiteral("repo"), repositoryUrl},
            {QStringLiteral("installCommand"), command},
            {QStringLiteral("installNote"), source.value(QStringLiteral("installNote")).toString()},
            {QStringLiteral("listedAt"), source.value(QStringLiteral("listedAt")).toString()},
            {QStringLiteral("tags"), source.value(QStringLiteral("tags")).toVariant()},
            {QStringLiteral("marketplaceUrl"), marketplaceUrl(id)}
        };
        plugins.append(plugin);
    }

    if (plugins.isEmpty()) {
        if (error)
            *error = QObject::tr("The marketplace catalog contained no usable community plugins.");
        return {};
    }
    if (error)
        error->clear();
    return plugins;
}

void Backend::refresh()
{
    if (m_loading)
        return;

    m_loading = true;
    emit loadingChanged();
    setError({});

    QNetworkRequest request(catalogUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Plugwatch/0.1.0"));
    request.setTransferTimeout(30000);
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_loading = false;
        emit loadingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            const QString message = tr("Could not check the marketplace: %1").arg(reply->errorString());
            reply->deleteLater();
            setError(message);
            emit checkFinished(false, message);
            return;
        }

        QString parseError;
        const QVariantList plugins = parseCatalog(reply->readAll(), &parseError);
        reply->deleteLater();
        if (plugins.isEmpty()) {
            setError(parseError);
            emit checkFinished(false, parseError);
            return;
        }

        QSet<QString> known;
        for (const auto &id : m_knownIds)
            known.insert(id);
        QSet<QString> stillListed;
        QStringList categories;
        QStringList newIds;
        for (const auto &value : plugins) {
            const auto plugin = value.toMap();
            const QString id = plugin.value(QStringLiteral("id")).toString();
            stillListed.insert(id);
            if (m_newIds.contains(id))
                newIds.append(id);
            if (!known.contains(id) && !m_knownIds.isEmpty())
                newIds.append(id);
            const QString category = plugin.value(QStringLiteral("category")).toString();
            if (!category.isEmpty())
                categories.append(category);
        }

        QStringList knownIds = stillListed.values();
        knownIds.sort(Qt::CaseInsensitive);
        m_knownIds = knownIds;
        m_newIds = sortedUnique(newIds);
        m_categories = sortedUnique(categories);
        m_plugins = plugins;
        m_lastChecked = QDateTime::currentDateTime().toString(Qt::ISODate);

        QString saveError;
        if (!saveState(&saveError)) {
            const QString message = tr("Catalog updated, but local state could not be saved: %1").arg(saveError);
            setError(message);
            emit pluginsChanged();
            emit checkFinished(false, message);
            return;
        }

        setError({});
        emit pluginsChanged();
        emit checkFinished(true, tr("Marketplace checked successfully."));
    });
}

void Backend::markNewAsSeen()
{
    if (m_newIds.isEmpty())
        return;
    const QStringList previous = m_newIds;
    m_newIds.clear();
    QString saveError;
    if (!saveState(&saveError)) {
        m_newIds = previous;
        setError(tr("Could not save your seen status: %1").arg(saveError));
    }
    emit pluginsChanged();
}

void Backend::markPluginAsSeen(const QString &pluginId)
{
    if (!m_newIds.removeAll(pluginId))
        return;
    QString saveError;
    if (!saveState(&saveError)) {
        m_newIds.append(pluginId);
        m_newIds = sortedUnique(m_newIds);
        setError(tr("Could not save your seen status: %1").arg(saveError));
    }
    emit pluginsChanged();
}

void Backend::copyInstallCommand(const QString &pluginId)
{
    for (const auto &value : m_plugins) {
        const auto plugin = value.toMap();
        if (plugin.value(QStringLiteral("id")).toString() != pluginId)
            continue;
        const QString command = plugin.value(QStringLiteral("installCommand")).toString();
        if (command.isEmpty()) {
            setError(tr("This listing does not provide an automatic install command."));
            return;
        }
        QGuiApplication::clipboard()->setText(command);
        setError({});
        setNotice(tr("Install command copied."));
        return;
    }
    setError(tr("That plugin is no longer in the current catalog."));
}

void Backend::loadState()
{
    QFile file(m_statePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;

    const auto state = document.object();
    m_lastChecked = state.value(QStringLiteral("lastChecked")).toString();
    for (const auto &value : state.value(QStringLiteral("knownIds")).toArray())
        if (value.isString())
            m_knownIds.append(value.toString());
    for (const auto &value : state.value(QStringLiteral("newIds")).toArray())
        if (value.isString())
            m_newIds.append(value.toString());
    const auto cached = state.value(QStringLiteral("plugins")).toArray();
    for (const auto &value : cached)
        if (value.isObject())
            m_plugins.append(value.toObject().toVariantMap());

    QStringList categories;
    for (const auto &plugin : m_plugins)
        categories.append(plugin.toMap().value(QStringLiteral("category")).toString());
    m_categories = sortedUnique(categories);
}

void Backend::loadWidgetState()
{
    const QString overridePath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/systemd/user/plugwatch-check.timer.d/50-plugwatch.conf");
    QFile overrideFile(overridePath);
    if (overrideFile.open(QIODevice::ReadOnly)) {
        static const QRegularExpression calendarPattern(
            QStringLiteral(R"(^OnCalendar=\*-\*-\* ((?:[01]\d|2[0-3]):[0-5]\d):00$)"),
            QRegularExpression::MultilineOption);
        const auto match = calendarPattern.match(QString::fromUtf8(overrideFile.readAll()));
        if (match.hasMatch()) {
            m_checkTime = match.captured(1);
            return;
        }
    }

    QFile file(m_widgetStatePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;
    const QString time = document.object().value(QStringLiteral("checkTime")).toString();
    if (isValidCheckTime(time))
        m_checkTime = time;
}

bool Backend::saveState(QString *error) const
{
    const QFileInfo info(m_statePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = tr("Could not create the application data directory.");
        return false;
    }

    QJsonArray known;
    for (const auto &id : m_knownIds)
        known.append(id);
    QJsonArray unseen;
    for (const auto &id : m_newIds)
        unseen.append(id);
    QJsonArray plugins;
    for (const auto &plugin : m_plugins)
        plugins.append(QJsonObject::fromVariantMap(plugin.toMap()));
    const QJsonObject state{
        {QStringLiteral("lastChecked"), m_lastChecked},
        {QStringLiteral("knownIds"), known},
        {QStringLiteral("newIds"), unseen},
        {QStringLiteral("plugins"), plugins}
    };

    QSaveFile file(m_statePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray contents = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (!saveWidgetState(error))
        return false;
    if (error)
        error->clear();
    return true;
}

bool Backend::saveWidgetState(QString *error) const
{
    const QFileInfo info(m_widgetStatePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = tr("Could not create the application data directory.");
        return false;
    }

    QJsonArray entries;
    constexpr int widgetEntryLimit = 100;
    for (const auto &pluginId : m_newIds.mid(0, widgetEntryLimit)) {
        for (const auto &value : m_plugins) {
            const auto plugin = value.toMap();
            if (plugin.value(QStringLiteral("id")).toString() != pluginId)
                continue;
            entries.append(QJsonObject{
                {QStringLiteral("id"), pluginId},
                {QStringLiteral("name"), plugin.value(QStringLiteral("name")).toString().left(160)},
                {QStringLiteral("category"), plugin.value(QStringLiteral("category")).toString().left(80)},
                {QStringLiteral("description"), plugin.value(QStringLiteral("description")).toString().left(400)},
                {QStringLiteral("thumbnailUrl"), plugin.value(QStringLiteral("thumbnailUrl")).toString()}
            });
            break;
        }
    }

    const QJsonObject widgetState{
        {QStringLiteral("unseenCount"), m_newIds.size()},
        {QStringLiteral("entries"), entries},
        {QStringLiteral("checkTime"), m_checkTime}
    };
    QSaveFile file(m_widgetStatePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray contents = QJsonDocument(widgetState).toJson(QJsonDocument::Compact);
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool Backend::saveWidgetCheckTime(QString *error) const
{
    QJsonObject state;
    QFile existing(m_widgetStatePath);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly)) {
            if (error)
                *error = existing.errorString();
            return false;
        }
        const auto document = QJsonDocument::fromJson(existing.readAll());
        if (!document.isObject()) {
            if (error)
                *error = tr("The shared widget state is invalid.");
            return false;
        }
        state = document.object();
    } else {
        state.insert(QStringLiteral("unseenCount"), 0);
        state.insert(QStringLiteral("entries"), QJsonArray{});
    }
    state.insert(QStringLiteral("checkTime"), m_checkTime);

    const QFileInfo info(m_widgetStatePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = tr("Could not create the application data directory.");
        return false;
    }
    QSaveFile file(m_widgetStatePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray contents = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (error)
        error->clear();
    return true;
}

void Backend::watchWidgetState()
{
    const QFileInfo info(m_widgetStatePath);
    if (!m_widgetWatcher.directories().contains(info.absolutePath()))
        m_widgetWatcher.addPath(info.absolutePath());
    if (QFileInfo::exists(m_widgetStatePath)
        && !m_widgetWatcher.files().contains(m_widgetStatePath))
        m_widgetWatcher.addPath(m_widgetStatePath);

    auto reload = [this] {
        const QString previousTime = m_checkTime;
        loadWidgetState();
        watchWidgetState();
        if (previousTime != m_checkTime)
            emit checkTimeChanged();
    };
    disconnect(&m_widgetWatcher, nullptr, this, nullptr);
    connect(&m_widgetWatcher, &QFileSystemWatcher::fileChanged, this, reload);
    connect(&m_widgetWatcher, &QFileSystemWatcher::directoryChanged, this, reload);
}

void Backend::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
    if (!error.isEmpty())
        setNotice({});
}

void Backend::setNotice(const QString &notice)
{
    if (m_notice == notice)
        return;
    m_notice = notice;
    emit noticeChanged();
}
