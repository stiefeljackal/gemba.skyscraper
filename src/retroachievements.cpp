#include "retroachievements.h"
#include "retroachievementsromhasher.h"

#include <QDebug>

RetroAchievements::RetroAchievements(Settings *config, QSharedPointer<NetManager> manager)
    : AbstractScraper(config, manager, MatchType::MATCH_ONE)
{
    qInfo() << "RetroAchievements module initialized (not implemented yet)";
}

void RetroAchievements::getSearchResults(QList<GameEntry> &gameEntries, QString searchName, QString platform) {
    Q_UNUSED(gameEntries)
    Q_UNUSED(searchName)
    Q_UNUSED(platform)

    // ROM hashing and game identification will be implemented here.
}

void RetroAchievements::runPasses(QList<GameEntry> &gameEntries, const QFileInfo &info, QString &output, QString &debug)
{
    Q_UNUSED(gameEntries)
    Q_UNUSED(debug)

    const std::uint32_t consoleId = RetroAchievementsRomHasher::consoleIdForPlatform(config->platform);

    if (consoleId == 0) {
        output.append(QStringLiteral("RetroAchievements: Unsupported platform '%1'\n").arg(config->platform));
        return;
    }

    const QString romPath = info.absoluteFilePath();

    const QString hash = RetroAchievementsRomHasher::generate(romPath, consoleId);

    if (hash.isEmpty()) {
        output.append(QStringLiteral("RetroAchievements: Failed to hash '%1'\n").arg(romPath));
        return;
    }

    output.append(QStringLiteral("RetroAchievements: ROM hash: %1\n").arg(hash));
}