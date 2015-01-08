import QtQuick 2.0
import QtQuick.Controls 1.0

import mpvtest 1.0

Item {
    width: 1280
    height: 720

    MpvObject {
        id: renderer

        // This object isn't real and not visible; it just renders into the
        // background of the containing Window.
        width: 0
        height: 0
    }

    MouseArea {
        anchors.fill: parent
        onClicked: renderer.command(["loadfile", "../../../test.mkv"])
    }

    Rectangle {
        id: labelFrame
        anchors.margins: -50
        radius: 5
        color: "white"
        border.color: "black"
        opacity: 0.8
        anchors.fill: box
    }

    Row {
        id: box
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 100

        Text {
            anchors.margins: 10
            wrapMode: Text.WordWrap
            text: "QtQuick and mpv are both rendering stuff.\n
                   In this example, mpv is always in the background.\n
                   Click to load ../../../test.mkv"
        }
    }
}
