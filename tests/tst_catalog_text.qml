import QtQuick
import QtTest
import "../src"

TestCase {
    name: "CatalogText"
    when: windowShown

    property string initialPluginId: ""
    property bool initialNewOnly: false
    property string markup: '<img src="https://attacker.invalid/pixel.png">'

    QtObject {
        id: theme
        property color accent: "#88aaff"
        property int radius: 0
    }

    QtObject {
        id: backend
        property var plugins: [{
            id: "sample",
            name: "Name " + markup,
            category: "Category " + markup,
            author: "Author " + markup,
            description: "Description " + markup,
            tags: ["Tag " + markup],
            installNote: "Installation " + markup,
            installCommand: "",
            thumbnailUrl: "",
            listedAt: ""
        }]
        property var categories: [plugins[0].category]
        property var newIds: []
        property string error: "Error " + markup
        property string notice: ""
        property string lastChecked: ""
        property string checkTime: "08:00"
        property bool loading: false
    }

    Main {
        id: catalog
    }

    function assertPlainText(item, observed) {
        if (item.text !== undefined && item.text.indexOf(markup) >= 0) {
            compare(item.textFormat, Text.PlainText, item.text)
            observed.push(item.text)
        }
        for (let child of item.children || [])
            assertPlainText(child, observed)
    }

    function test_catalogMarkupIsLiteral() {
        let observed = []
        assertPlainText(catalog.contentItem, observed)
        for (let field of ["Name", "Category", "Author", "Description", "Tag", "Installation", "Error"])
            verify(observed.some(text => text.indexOf(field + " " + markup) >= 0),
                   "Missing rendered field: " + field)
    }
}
