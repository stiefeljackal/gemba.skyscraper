#ifndef DEFAULTENTRYSUMMARYFORMATTER_H
#define DEFAULTENTRYSUMMARYFORMATTER_H

#include "entrysummaryformatter.h"

class DefaultEntrySummaryFormatter final : public EntrySummaryFormatter {
public:
    void appendSummary(QString &output, const GameEntry &game,
                       const Settings &config,
                       const EntrySummaryContext &context) const override;
};

#endif // DEFAULTENTRYSUMMARYFORMATTER_H