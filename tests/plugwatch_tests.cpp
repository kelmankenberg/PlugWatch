#include <QtTest>

#include "backend.h"
#include "plugwatch_tests.h"

void PlugwatchTests::parsesCommunityPluginsAndInstallDetails()
{
    const QByteArray json = R"json({
          "plugins": [
            {
              "id": "sample.one",
              "name": "Sample",
              "description": "A test plugin",
              "category": "Desktop",
              "author": "Contributor",
              "sourceType": "community",
              "repo": "https://github.com/example/sample",
              "previewThumbnail": "assets/img/plugins/sample-card.webp",
              "installAvailable": true,
              "installCommand": "omarchy plugin add https://github.com/example/sample.git --enable",
              "tags": ["media"]
            },
            {
              "id": "builtin.one",
              "name": "Built in",
              "sourceType": "builtin"
            }
          ]
        })json";
    QString error;
    const auto plugins = Backend::parseCatalog(json, &error);
    QCOMPARE(error, QString());
    QCOMPARE(plugins.size(), 1);
    const auto plugin = plugins.first().toMap();
    QCOMPARE(plugin.value("id").toString(), QString("sample.one"));
    QCOMPARE(plugin.value("category").toString(), QString("Desktop"));
    QCOMPARE(plugin.value("installCommand").toString(),
             QString("omarchy plugin add https://github.com/example/sample.git --enable"));
    QCOMPARE(plugin.value("marketplaceUrl").toString(),
             QString("https://plugins.omarchy.org/plugin.html?id=sample.one"));
    QCOMPARE(plugin.value("thumbnailUrl").toString(),
             QString("https://plugins.omarchy.org/assets/img/plugins/sample-card.webp"));
}

void PlugwatchTests::manualSetupUsesRepositoryWithoutAnInstallCommand()
{
    const QByteArray json = R"json({"plugins":[{
          "id":"manual.one",
          "name":"Manual",
          "sourceType":"community",
          "repo":"https://github.com/example/manual",
          "previewThumbnail":"https://attacker.example/image.png",
          "installAvailable":false,
          "installCommand":"should not be shown",
          "installNote":"Follow the repository guide."
        }]})json";
    QString error;
    const auto plugins = Backend::parseCatalog(json, &error);
    QCOMPARE(plugins.size(), 1);
    const auto plugin = plugins.first().toMap();
    QVERIFY(plugin.value("installCommand").toString().isEmpty());
    QCOMPARE(plugin.value("installNote").toString(), QString("Follow the repository guide."));
    QCOMPARE(plugin.value("repo").toString(), QString("https://github.com/example/manual"));
    QVERIFY(plugin.value("thumbnailUrl").toString().isEmpty());
}

void PlugwatchTests::rejectsMalformedOrEmptyCatalogs()
{
    QString error;
    QVERIFY(Backend::parseCatalog("{", &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(Backend::parseCatalog(R"({"plugins":[]})", &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void PlugwatchTests::validatesCheckTimes()
{
    QVERIFY(Backend::isValidCheckTime("00:00"));
    QVERIFY(Backend::isValidCheckTime("08:30"));
    QVERIFY(Backend::isValidCheckTime("23:59"));
    QVERIFY(!Backend::isValidCheckTime("24:00"));
    QVERIFY(!Backend::isValidCheckTime("8:00"));
    QVERIFY(!Backend::isValidCheckTime("08:60"));
    QVERIFY(!Backend::isValidCheckTime("08:00\n"));
}

QTEST_APPLESS_MAIN(PlugwatchTests)
