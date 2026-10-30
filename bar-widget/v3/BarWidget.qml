import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

BarWidget {
  id: root
  moduleName: "plugwatch.bar-widget"

  readonly property string dataHome: Quickshell.env("XDG_DATA_HOME")
    || (Quickshell.env("HOME") + "/.local/share")
  readonly property string statePath: dataHome + "/plugwatch/widget.json"
  property int unseenCount: 0
  property var entries: []
  property string checkTime: "08:00"
  property string scheduleError: ""
  property bool popupOpen: false
  readonly property bool opened: popupOpen
  readonly property int entryLimit: 100

  function readState(contents) {
    if (!contents || contents.length > 200000) {
      unseenCount = 0
      entries = []
      return
    }
    try {
      const state = JSON.parse(contents)
      const count = Number(state.unseenCount)
      const rows = Array.isArray(state.entries) ? state.entries : []
      const time = String(state.checkTime || "08:00")
      unseenCount = Number.isSafeInteger(count) && count >= 0 ? count : 0
      checkTime = /^(?:[01]\d|2[0-3]):[0-5]\d$/.test(time) ? time : "08:00"
      entries = rows.slice(0, entryLimit).filter(function(row) {
        return row && /^[a-z0-9._-]{1,160}$/i.test(String(row.id || ""))
      }).map(function(row) {
        return {
          id: String(row.id).slice(0, 160),
          name: String(row.name || "").slice(0, 160),
          category: String(row.category || "").slice(0, 80),
          description: String(row.description || "").slice(0, 400),
          thumbnailUrl: isTrustedThumbnail(row.thumbnailUrl) ? String(row.thumbnailUrl) : ""
        }
      })
    } catch (error) {
      unseenCount = 0
      entries = []
    }
  }

  function isTrustedThumbnail(value) {
    const url = String(value || "")
    return /^https:\/\/plugins\.omarchy\.org\/assets\/img\/plugins\/[A-Za-z0-9._/-]+\.(webp|png|jpe?g|avif)$/i.test(url)
      && url.indexOf("..") < 0
  }

  function refresh() {
    stateFile.reload()
  }

  function open() {
    popupOpen = true
  }

  function close() {
    popupOpen = false
  }

  function togglePanel() {
    popupOpen = !popupOpen
  }

  function launchApp(pluginId, newOnly) {
    if (!root.bar) return
    let command = "plugwatch"
    if (newOnly) command += " --new"
    if (pluginId) command += " --plugin " + Util.shellQuote(pluginId)
    popupOpen = false
    root.bar.run(command)
  }

  FileView {
    id: stateFile
    path: root.statePath
    watchChanges: true
    printErrors: false
    onFileChanged: reload()
    onLoaded: {
      root.readState(text())
      checkTimeField.text = root.checkTime
    }
    onLoadFailed: {
      root.unseenCount = 0
      root.entries = []
    }
  }

  Process {
    id: timeSetter
    command: ["plugwatch", "--set-check-time", checkTimeField.text]
    stdout: StdioCollector {}
    stderr: StdioCollector {}
    onExited: function(exitCode, exitStatus) {
      if (exitCode !== 0) {
        root.scheduleError = strings.t("widget.timeFailed")
        return
      }
      root.scheduleError = ""
      root.refresh()
    }
  }

  Timer {
    interval: 60000
    running: true
    repeat: true
    onTriggered: root.refresh()
  }

  ShellIpc {
    target: "plugwatch.bar-widget"
    function refresh() { root.refresh() }
    function open() { root.open() }
    function close() { root.close() }
    function toggle() { root.togglePanel() }
  }

  visible: true
  implicitWidth: button.implicitWidth
  implicitHeight: barSize

  Commons.Translations {
    id: strings
    directory: Qt.resolvedUrl("../i18n")
  }

  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.vertical || root.unseenCount === 0
      ? "󰏖"
      : "󰏖 " + Commons.I18n.formatNumber(root.unseenCount)
    fontSize: Style.font.body
    horizontalMargin: 7
    tooltipText: strings.tn("widget.unseen", root.unseenCount)
      .replace("%1", Commons.I18n.formatNumber(root.unseenCount))
    onPressed: root.togglePanel()
  }

  PopupCard {
    id: popup
    anchorItem: button
    bar: root.bar
    owner: root
    open: root.popupOpen
    contentWidth: popup.fittedContentWidth(Style.space(360))
    contentHeight: popup.fittedContentHeight(contentColumn.implicitHeight)

    Column {
      id: contentColumn
      width: parent.width
      spacing: Style.space(10)

      RowLayout {
        width: parent.width
        spacing: Style.space(8)
        ColumnLayout {
          Layout.fillWidth: true
          spacing: Style.space(2)
          Text {
            textFormat: Text.PlainText
            text: strings.t("widget.title")
            color: Color.popups.text
            font.family: root.bar ? root.bar.fontFamily : Style.font.family
            font.pixelSize: Style.font.subtitle
            font.bold: true
          }
          Text {
            textFormat: Text.PlainText
            text: strings.tn("widget.unseen", root.unseenCount)
                .replace("%1", Commons.I18n.formatNumber(root.unseenCount))
            color: Color.popups.text
            opacity: 0.72
            font.family: root.bar ? root.bar.fontFamily : Style.font.family
            font.pixelSize: Style.font.bodySmall
          }
        }
        WidgetButton {
          text: "󰖟"
          bar: root.bar
          tooltipText: strings.t("widget.openFullApp")
          onPressed: root.launchApp("", false)
        }
      }

      Column {
        width: parent.width
        spacing: Style.space(4)
        Text {
          textFormat: Text.PlainText
          text: strings.t("widget.checkTime")
          color: Color.popups.text
          font.family: root.bar ? root.bar.fontFamily : Style.font.family
          font.pixelSize: Style.font.bodySmall
        }
        Column {
          width: parent.width
          spacing: Style.space(6)
          TextField {
            id: checkTimeField
            width: parent.width
            text: root.checkTime
            placeholderText: "HH:MM"
            inputMethodHints: Qt.ImhTime
            validator: RegularExpressionValidator {
              regularExpression: /^(?:[01][0-9]|2[0-3]):[0-5][0-9]$/
            }
            onAccepted: if (acceptableInput && text !== root.checkTime && !timeSetter.running)
                          timeSetter.running = true
          }
          Button {
            text: timeSetter.running ? strings.t("widget.savingTime") : strings.t("widget.saveTime")
            width: parent.width
            enabled: checkTimeField.acceptableInput
                     && checkTimeField.text !== root.checkTime
                     && !timeSetter.running
            onClicked: {
              root.scheduleError = ""
              timeSetter.running = true
            }
          }
        }
      }
      Text {
        visible: root.scheduleError !== ""
        width: parent.width
        textFormat: Text.PlainText
        text: root.scheduleError
        color: "#ff8a80"
        font.family: root.bar ? root.bar.fontFamily : Style.font.family
        font.pixelSize: Style.font.bodySmall
        wrapMode: Text.Wrap
      }

      Rectangle {
        width: parent.width
        height: 1
        color: Color.popups.border
      }

      Text {
        visible: root.unseenCount === 0
        width: parent.width
        textFormat: Text.PlainText
        text: strings.t("widget.empty").replace("%1", root.checkTime)
        color: Color.popups.text
        opacity: 0.72
        font.family: root.bar ? root.bar.fontFamily : Style.font.family
        font.pixelSize: Style.font.bodySmall
        wrapMode: Text.Wrap
      }

      Column {
        width: parent.width
        spacing: Style.space(4)
        Repeater {
          model: root.entries
          delegate: Rectangle {
            id: entryRow
            required property var modelData
            width: parent.width
            height: Style.space(56)
            radius: Math.min(4, Style.cornerRadius)
            color: entryMouse.containsMouse
              ? Style.hoverFillFor(Color.popups.text, Commons.Color.accent)
              : "transparent"
            RowLayout {
              anchors.fill: parent
              anchors.leftMargin: Style.space(6)
              anchors.rightMargin: Style.space(6)
              spacing: Style.space(8)
              Rectangle {
                Layout.preferredWidth: Style.space(48)
                Layout.preferredHeight: Style.space(38)
                visible: entryRow.modelData.thumbnailUrl !== ""
                color: Color.popups.background
                radius: Math.min(4, Style.cornerRadius)
                clip: true
                Image {
                  anchors.fill: parent
                  source: entryRow.modelData.thumbnailUrl
                  fillMode: Image.PreserveAspectCrop
                  asynchronous: true
                  cache: true
                  visible: status === Image.Ready
                }
              }
              ColumnLayout {
                Layout.fillWidth: true
                spacing: Style.space(2)
                Text {
                  Layout.fillWidth: true
                  textFormat: Text.PlainText
                  text: entryRow.modelData.name
                  color: Color.popups.text
                  font.family: root.bar ? root.bar.fontFamily : Style.font.family
                  font.pixelSize: Style.font.body
                  font.bold: true
                  elide: Text.ElideRight
                }
                Text {
                  Layout.fillWidth: true
                  textFormat: Text.PlainText
                  text: entryRow.modelData.category
                  color: Color.popups.text
                  opacity: 0.7
                  font.family: root.bar ? root.bar.fontFamily : Style.font.family
                  font.pixelSize: Style.font.bodySmall
                  elide: Text.ElideRight
                }
              }
              Text {
                textFormat: Text.PlainText
                text: "NEW"
                color: Commons.Color.accent
                font.family: root.bar ? root.bar.fontFamily : Style.font.family
                font.pixelSize: Style.font.bodySmall
                font.bold: true
              }
            }
            MouseArea {
              id: entryMouse
              anchors.fill: parent
              hoverEnabled: true
              cursorShape: Qt.PointingHandCursor
              onClicked: root.launchApp(entryRow.modelData.id, false)
            }
          }
        }
      }

      Text {
        visible: root.unseenCount > root.entries.length
        width: parent.width
        textFormat: Text.PlainText
        text: strings.t("widget.moreInApp")
          .replace("%1", Commons.I18n.formatNumber(root.unseenCount - root.entries.length))
        color: Color.popups.text
        opacity: 0.72
        font.family: root.bar ? root.bar.fontFamily : Style.font.family
        font.pixelSize: Style.font.bodySmall
      }

      Button {
        width: parent.width
        text: strings.t("widget.openAll")
        enabled: root.unseenCount > 0
        onClicked: root.launchApp("", true)
      }
    }
  }
}
