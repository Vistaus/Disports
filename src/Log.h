#pragma once

#include <QLoggingCategory>

// Warnings are always shown; debug messages only when enabled, e.g.
// QT_LOGGING_RULES="disports.voice.debug=true".
Q_DECLARE_LOGGING_CATEGORY(lcCore)
Q_DECLARE_LOGGING_CATEGORY(lcGateway)
Q_DECLARE_LOGGING_CATEGORY(lcVideo)
Q_DECLARE_LOGGING_CATEGORY(lcVoice)
