#ifndef RETROACHIEVEMENTS_H
#define RETROACHIEVEMENTS_H

#include "abstractscraper.h"

class RetroAchievements final : public AbstractScraper {
public:
    RetroAchievements(Settings *config, QSharedPointer<NetManager> manager);
    void runPasses(QList<GameEntry> &gameEntries, const QFileInfo &info, QString &output, QString &debug) override;

private:
    void getSearchResults(QList<GameEntry> &gameEntries, QString searchName, QString platform) override;
};

#endif // RETROACHIEVEMENTS_H