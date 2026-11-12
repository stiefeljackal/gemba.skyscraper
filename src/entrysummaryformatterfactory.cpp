#include "entrysummaryformatterfactory.h"

#include "defaultentrysummaryformatter.h"
#include "entrysummaryformatter.h"

#ifdef WITH_RETROACHIEVEMENTS
#include "retroachievementsentrysummaryformatter.h"
#endif

std::unique_ptr<EntrySummaryFormatter>
EntrySummaryFormatterFactory::create(const QString &scraper) {
#ifdef WITH_RETROACHIEVEMENTS
    if (scraper == "retroachievements") {
        return std::make_unique<RetroAchievementsEntrySummaryFormatter>();
    }
#else
    Q_UNUSED(scraper)
#endif

    return std::make_unique<DefaultEntrySummaryFormatter>();
}