#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QTranslator>

class Translator : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY changed)

public:
    explicit Translator(QObject *parent = nullptr);
    QString language() const { return m_language; }
    bool isEmpty() const override { return false; }
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;

signals:
    void changed();

private:
    void reload();
    QString m_languageFile;
    QString m_language = QStringLiteral("en");
    QHash<QString, QString> m_strings;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
