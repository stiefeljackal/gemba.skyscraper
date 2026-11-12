
#include "entrysummaryformatter.h"

#include "gameentry.h"
#include "settings.h"

#ifdef WITH_RETROACHIEVEMENTS
#include "retroachievementsentrysummaryformatter.h"
#endif

#include <QDate>

void EntrySummaryFormatter::appendField(QString &output, const QString &label,
                                        const QString &value) {
    output.append(label.leftJustified(16) + value + "\n");
}

void EntrySummaryFormatter::appendSourcedField(QString &output,
                                               const QString &label,
                                               const QString &value,
                                               const QString &source) {
    output.append(label.leftJustified(16) + "'\033[1;32m" + value +
                  "\033[0m' (" + source + ")\n");
}

void EntrySummaryFormatter::appendMediaStatus(QString &output,
                                              const QString &label,
                                              bool available,
                                              const QString &source,
                                              bool uncached) {
    output.append(label.leftJustified(16) +
                  QString(available ? "\033[1;32mYES" : "\033[1;31mNO") +
                  "\033[0m" + (uncached ? " (uncached)" : "") + " (" + source +
                  ")\n");
}
