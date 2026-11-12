
#ifndef RETROACHIEVEMENTSENTRYSUMMARYFORMATTER_H
#define RETROACHIEVEMENTSENTRYSUMMARYFORMATTER_H

#include "entrysummaryformatter.h"

class RetroAchievementsEntrySummaryFormatter final
    : public EntrySummaryFormatter {
public:
    void appendSummary(QString &output, const GameEntry &game,
                       const Settings &config,
                       const EntrySummaryContext &context) const override;
};

#endif // RETROACHIEVEMENTSENTRYSUMMARYFORMATTER_H
