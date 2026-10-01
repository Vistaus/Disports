import QtQuick
import QtWebEngine
import Lomiri.Components
import Disports.Core

// Discord's captcha (hCaptcha) for Session.captcha. Discord's site key only
// works on discord.com, so the page with the widget says it is there. The
// solution comes back in the page's title. Uses the system's Qt WebEngine
// (Ubuntu Touch 24.04-2.x).
Page {
    id: captchaPage
    objectName: "captchaPage"

    header: PageHeader {
        title: i18n.tr("Are you human?")
        leadingActionBar.actions: [
            Action {
                iconName: "back"
                text: i18n.tr("Cancel")
                onTriggered: Session.captcha.cancel()
            }
        ]
    }

    readonly property bool dark: theme.palette.normal.background.hslLightness < 0.5

    function html() {
        const settings = {
            sitekey: Session.captcha.siteKey,
            theme: dark ? "dark" : "light",
            size: Session.captcha.invisible ? "invisible" : "normal",
        }
        return "<!DOCTYPE html><html><head>"
            + "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            + "<style>body { margin: 0; padding-top: 16px; display: flex; justify-content: center; background: "
            + theme.palette.normal.background + "; }</style>"
            + "<script src='https://js.hcaptcha.com/1/api.js?render=explicit&onload=ready&recaptchacompat=off' async defer></script>"
            + "</head><body><div id='box'></div><script>"
            + "function ready() {"
            + "  var settings = " + JSON.stringify(settings) + ";"
            + "  settings.callback = function(solution) { document.title = 'solved:' + solution; };"
            + "  settings['error-callback'] = function(error) { document.title = 'error:' + error; };"
            + "  var id = hcaptcha.render('box', settings);"
            + "  var rqdata = " + JSON.stringify(Session.captcha.rqdata) + ";"
            + "  if (rqdata) hcaptcha.setData(id, { rqdata: rqdata });"
            + "  if (settings.size === 'invisible') hcaptcha.execute(id);"
            + "}"
            + "</script></body></html>"
    }

    function load() {
        failed.visible = false
        web.loadHtml(html(), "https://discord.com/")
    }

    // The next captcha in line, if another one came while this was open.
    Connections {
        target: Session.captcha
        function onRequested() { captchaPage.load() }
    }

    Label {
        id: intro
        anchors {
            top: captchaPage.header.bottom
            left: parent.left
            right: parent.right
            margins: units.gu(2)
        }
        text: i18n.tr("Discord wants to make sure you're a person. Solve the captcha to continue.")
        wrapMode: Text.WordWrap
        color: theme.palette.normal.backgroundSecondaryText
    }

    Label {
        id: failed
        anchors {
            top: intro.bottom
            left: parent.left
            right: parent.right
            margins: units.gu(2)
        }
        visible: false
        text: i18n.tr("The captcha couldn't be loaded. Check your internet connection.")
        wrapMode: Text.WordWrap
        color: theme.palette.normal.negative
    }

    WebEngineView {
        id: web
        anchors {
            top: failed.visible ? failed.bottom : intro.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            topMargin: units.gu(1)
        }
        backgroundColor: theme.palette.normal.background
        // Nothing kept: no cookies or cache from the captcha.
        profile: WebEngineProfile {
            offTheRecord: true
            httpUserAgent: Session.userAgent()
        }

        onTitleChanged: {
            if (title.indexOf("solved:") === 0)
                Session.captcha.solve(title.substring(7))
            else if (title.indexOf("error:") === 0)
                failed.visible = true
        }
        onLoadingChanged: function(request) {
            if (request.status === WebEngineView.LoadFailedStatus)
                failed.visible = true
        }
        // hCaptcha's links (privacy, help) open in the browser.
        onNewWindowRequested: function(request) { Qt.openUrlExternally(request.requestedUrl) }

        Component.onCompleted: captchaPage.load()
    }
}
