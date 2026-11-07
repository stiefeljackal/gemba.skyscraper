#include "retroachievements.h"

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