#pragma once

#include <QObject>

class PlugwatchTests : public QObject {
    Q_OBJECT

private slots:
    void parsesCommunityPluginsAndInstallDetails();
    void manualSetupUsesRepositoryWithoutAnInstallCommand();
    void rejectsMalformedOrEmptyCatalogs();
    void validatesCheckTimes();
};
