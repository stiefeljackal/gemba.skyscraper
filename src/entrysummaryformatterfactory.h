#ifndef ENTRYSUMMARYFORMATTERFACTORY_H
#define ENTRYSUMMARYFORMATTERFACTORY_H

#include <QString>
#include <memory>

class EntrySummaryFormatter;

class EntrySummaryFormatterFactory final {
public:
    static std::unique_ptr<EntrySummaryFormatter>
    create(const QString &scraper);
};

#endif // ENTRYSUMMARYFORMATTERFACTORY_H