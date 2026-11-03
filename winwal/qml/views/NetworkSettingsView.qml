import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property string pageTitle: ""
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        Text {
            text: pageTitle
            color: "#ffffff"
            font.pixelSize: 22
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: Config ? (Config.language, Config.trAuto("Модуль будет наполнен на следующем шаге")) : "Модуль будет наполнен на следующем шаге"
            color: "#666666"
            font.pixelSize: 13
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
