#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QVariantList>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QStringList categories READ categories NOTIFY pluginsChanged)
    Q_PROPERTY(QStringList newIds READ newIds NOTIFY pluginsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY pluginsChanged)
    Q_PROPERTY(QString checkTime READ checkTime NOTIFY checkTimeChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    QVariantList plugins() const { return m_plugins; }
    QStringList categories() const { return m_categories; }
    QStringList newIds() const { return m_newIds; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    QString lastChecked() const { return m_lastChecked; }
    QString checkTime() const { return m_checkTime; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setCheckTime(const QString &time);
    Q_INVOKABLE void markNewAsSeen();
    Q_INVOKABLE void markPluginAsSeen(const QString &pluginId);
    Q_INVOKABLE void copyInstallCommand(const QString &pluginId);
    Q_INVOKABLE static QString marketplaceUrl(const QString &pluginId);
    static bool isValidCheckTime(const QString &time);
    static QVariantList parseCatalog(const QByteArray &json, QString *error = nullptr);

signals:
    void pluginsChanged();
    void loadingChanged();
    void errorChanged();
    void noticeChanged();
    void checkTimeChanged();
    void checkFinished(bool success, const QString &message);

private:
    void loadState();
    void loadWidgetState();
    bool saveState(QString *error = nullptr) const;
    bool saveWidgetState(QString *error = nullptr) const;
    bool saveWidgetCheckTime(QString *error = nullptr) const;
    void watchWidgetState();
    void setError(const QString &error);
    void setNotice(const QString &notice);

    QNetworkAccessManager m_network;
    QVariantList m_plugins;
    QStringList m_categories;
    QStringList m_knownIds;
    QStringList m_newIds;
    QString m_error;
    QString m_notice;
    QString m_lastChecked;
    QString m_statePath;
    QString m_widgetStatePath;
    QString m_checkTime = QStringLiteral("08:00");
    QFileSystemWatcher m_widgetWatcher;
    bool m_loading = false;
};
