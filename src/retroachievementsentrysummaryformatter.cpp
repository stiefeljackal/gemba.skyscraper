
#include "retroachievementsentrysummaryformatter.h"

#include "gameentry.h"
#include "retroachievementsentrysummaryformatter.h"
#include "settings.h"

#include <QFileInfo>

void RetroAchievementsEntrySummaryFormatter::appendSummary(
    QString &output, const GameEntry &game, const Settings &config,
    const EntrySummaryContext &context) const {
    Q_UNUSED(config)

    appendField(output, "Scraper:", "retroachievements");

    appendField(output, "From cache:", context.fromCache ? "YES" : "NO");

    appendField(output,
                "ROM File:", QFileInfo(game.absoluteFilePath).fileName());

    appendField(output, "Platform:", game.platform);

    appendField(output, "ROM Hash:",
                game.raHash.isEmpty() ? "Unavailable" : game.raHash);

    const QString gameId =
        game.raGameId == "0" ? QStringLiteral("Not identified") : game.raGameId;

    appendField(output,
                "RA Game ID:", gameId.isEmpty() ? "Unavailable" : gameId);
}
