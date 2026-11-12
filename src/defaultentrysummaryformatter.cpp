#include "defaultentrysummaryformatter.h"

#include "gameentry.h"
#include "settings.h"

#include <QDate>

void DefaultEntrySummaryFormatter::appendSummary(
    QString &output, const GameEntry &game, const Settings &config,
    const EntrySummaryContext &context) const {
    QString scraperName = config.scraper;

    if (scraperName == "worldofspectrum") {
        scraperName = "zxinfo (formerly: worldofspectrum)";
    }

    output.append("Scraper:        " + scraperName + "\n");

    if (!context.cacheScraper && config.scraper != "import") {
        output.append(
            "From cache:     " +
            QString(context.fromCache
                        ? "YES (refresh from source with '--cache refresh')"
                        : "NO") +
            "\n");

        output.append(
            "Search match:   " + QString::number(context.searchMatch) + " %\n");

        output.append("Compare title:  '\033[1;32m" + context.compareTitle +
                      "\033[0m'\n");

        output.append("Result title:   '\033[1;32m" + context.resultTitle +
                      "\033[0m' (" + game.titleSrc + ")\n");
    } else {
        output.append("Title:          '\033[1;32m" + context.resultTitle +
                      "\033[0m' (" + game.titleSrc + ")\n");
    }

    appendSourcedField(output, "Platform:", game.platform, game.platformSrc);

    const QString releaseDate =
        game.releaseDate.isEmpty()
            ? QString()
            : QDate::fromString(game.releaseDate, "yyyyMMdd")
                  .toString("yyyy-MM-dd");

    appendSourcedField(output, "Release Date:", releaseDate,
                       game.releaseDate.isEmpty() ? QString()
                                                  : game.releaseDateSrc);

    appendSourcedField(output, "Developer:", game.developer, game.developerSrc);

    appendSourcedField(output, "Publisher:", game.publisher, game.publisherSrc);

    appendSourcedField(output, "Players:", game.players, game.playersSrc);

    appendSourcedField(output,
                       "Ages:", game.ages + (game.ages.toInt() != 0 ? "+" : ""),
                       game.agesSrc);

    appendSourcedField(output, "Tags:", game.tags, game.tagsSrc);

    appendSourcedField(output, "Rating (0-1):", game.rating, game.ratingSrc);

    // Media availability
    appendMediaStatus(
        output, "Cover:", !game.coverSrc.isEmpty() || !game.coverData.isNull(),
        game.coverSrc, !config.cacheCovers && !context.cacheScraper);

    appendMediaStatus(
        output, "Screenshot:",
        !game.screenshotSrc.isEmpty() || !game.screenshotData.isNull(),
        game.screenshotSrc, !config.cacheScreenshots && !context.cacheScraper);

    appendMediaStatus(
        output, "Wheel:", !game.wheelSrc.isEmpty() || !game.wheelData.isEmpty(),
        game.wheelSrc, !config.cacheWheels && !context.cacheScraper);

    appendMediaStatus(
        output,
        "Marquee:", !game.marqueeSrc.isEmpty() || !game.marqueeData.isNull(),
        game.marqueeSrc, !config.cacheMarquees && !context.cacheScraper);

    appendMediaStatus(
        output,
        "Texture:", !game.textureSrc.isEmpty() || !game.textureData.isNull(),
        game.textureSrc, !config.cacheTextures && !context.cacheScraper);

    if (config.videos) {
        output.append("Video:          " +
                      QString(game.videoFormat.isEmpty() ? "\033[1;31mNO"
                                                         : "\033[1;32mYES") +
                      "\033[0m" +
                      QString(game.videoSize <= config.videoSizeLimit
                                  ? ""
                                  : " (size exceeded, uncached)") +
                      " (" + game.videoSrc + ")\n");
    }

    if (config.manuals) {
        appendMediaStatus(
            output,
            "Manual:", !game.manualSrc.isEmpty() || !game.manualData.isEmpty(),
            game.manualSrc);
    }

    if (config.fanart) {
        appendMediaStatus(
            output,
            "Fanart:", !game.fanartSrc.isEmpty() || !game.fanartData.isEmpty(),
            game.fanartSrc);
    }

    if (config.backcovers) {
        appendMediaStatus(output, "Backcover:",
                          !game.backcoverSrc.isEmpty() ||
                              !game.backcoverData.isEmpty(),
                          game.backcoverSrc);
    }

    output.append("\nDescription: (" + game.descriptionSrc + ")\n'\033[1;32m" +
                  game.description.left(config.maxLength) + "\033[0m'\n");
}