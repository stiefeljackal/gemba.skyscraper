
#ifndef ENTRYSUMMARYFORMATTER_H
#define ENTRYSUMMARYFORMATTER_H

#include <QString>
#include <memory>

class GameEntry;
struct Settings;

struct EntrySummaryContext {
    QString compareTitle;
    QString resultTitle;

    int searchMatch = 0;
    bool fromCache = false;
    bool cacheScraper = false;
};

class EntrySummaryFormatter {
public:
    virtual ~EntrySummaryFormatter() = default;

    virtual void appendSummary(QString &output, const GameEntry &game,
                               const Settings &config,
                               const EntrySummaryContext &context) const = 0;

protected:
    static void appendField(QString &output, const QString &label,
                            const QString &value);

    static void appendSourcedField(QString &output, const QString &label,
                                   const QString &value, const QString &source);

    static void appendMediaStatus(QString &output, const QString &label,
                                  bool available, const QString &source,
                                  bool uncached = false);
};

#endif // ENTRYSUMMARYFORMATTER_H
