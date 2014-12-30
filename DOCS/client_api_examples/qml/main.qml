import QtQuick 2.0
import QtQuick.Controls 1.0

import mpvtest 1.0

Item {
    width: 1280
    height: 720

    MpvObject {
        id: renderer
        anchors.fill: parent

        MouseArea {
            anchors.fill: parent
            onClicked: renderer.loadfile("../../../test.mkv")
        }
    }

    Rectangle {
        id: labelFrame
        anchors.margins: -50
        radius: 5
        color: "white"
        border.color: "black"
        opacity: 0.8
        anchors.fill: label
    }

    Text {
        id: label
        anchors.bottom: renderer.bottom
        anchors.left: renderer.left
        anchors.right: renderer.right
        anchors.margins: 100
        wrapMode: Text.WordWrap
        text: "QtQuick and mpv are both rendering stuff.\n
               Click to load ../../../test.mkv"
    }
}
