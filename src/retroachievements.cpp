
#include "retroachievements.h"

#include "rc_api_runtime.h"
#include "retroachievementsrequestscheduler.h"
#include "retroachievementsromhasher.h"
#include "retroachievementsruntime.h"

#include <QDebug>
#include <QEventLoop>
#include <QFileInfo>
#include <QMetaObject>
#include <atomic>
#include <cstdint>

namespace {

    std::atomic<quint64> nextRequestId{1};

} // namespace

RetroAchievements::RetroAchievements(Settings *config,
                                     QSharedPointer<NetManager> manager)
    : AbstractScraper(config, manager, MatchType::MATCH_ONE) {
    RetroAchievementsRuntime::instance();

    qInfo() << "RetroAchievements module initialized";
}

void RetroAchievements::runPasses(QList<GameEntry> &gameEntries,
                                  const QFileInfo &info, QString &output,
                                  QString &debug) {
    Q_UNUSED(debug)

    const std::uint32_t consoleId =
        RetroAchievementsRomHasher::consoleIdForPlatform(config->platform);

    if (consoleId == 0) {
        output.append(
            QStringLiteral("RetroAchievements: Unsupported platform '%1'\n")
                .arg(config->platform));
        return;
    }

    const QString romPath = info.absoluteFilePath();

    const QString hash =
        RetroAchievementsRomHasher::generate(romPath, consoleId);

    if (hash.isEmpty()) {
        output.append(QStringLiteral("RetroAchievements: Failed to hash '%1'\n")
                          .arg(romPath));
        return;
    }

    // Construct the request using the rcheevos API.
    const QByteArray hashBytes = hash.toUtf8();

    rc_api_resolve_hash_request_t params{};
    params.game_hash = hashBytes.constData();

    rc_api_request_t request{};

    const int initResult = rc_api_init_resolve_hash_request(&request, &params);

    if (initResult != RC_OK) {
        output.append(QStringLiteral("RetroAchievements: Request construction "
                                     "failed: %1\n")
                          .arg(QString::fromUtf8(rc_error_str(initResult))));

        rc_api_destroy_request(&request);
        return;
    }

    // Copy request data before destroying the rcheevos request.
    const QString url = QString::fromUtf8(request.url);

    QByteArray postData;

    if (request.post_data != nullptr) {
        postData = QByteArray(request.post_data);
    }

    rc_api_destroy_request(&request);

    RetroAchievementsRequestScheduler *scheduler =
        RetroAchievementsRuntime::instance().scheduler();

    if (!scheduler) {
        output.append(
            QStringLiteral("RetroAchievements: Scheduler unavailable\n"));
        return;
    }

    const quint64 requestId =
        nextRequestId.fetch_add(1, std::memory_order_relaxed);

    QEventLoop waitLoop;
    RaHttpResult httpResult;

    const auto connection = QObject::connect(
        scheduler, &RetroAchievementsRequestScheduler::requestFinished,
        &waitLoop,
        [&](quint64 completedId, const RaHttpResult &result) {
            if (completedId != requestId) {
                return;
            }

            httpResult = result;
            waitLoop.quit();
        },
        Qt::QueuedConnection);

    const bool queued = QMetaObject::invokeMethod(
        scheduler,
        [scheduler, requestId, hash, url, postData]() {
            scheduler->enqueue(requestId, hash, url, postData);
        },
        Qt::QueuedConnection);

    if (!queued) {
        QObject::disconnect(connection);

        output.append(
            QStringLiteral("RetroAchievements: Failed to queue request\n"));
        return;
    }

    // The nested event loop lets this worker receive its result while
    // the scheduler continues processing requests independently.
    waitLoop.exec();

    QObject::disconnect(connection);

    if (httpResult.status == RaRequestStatus::RateLimited) {
        output.append(
            QStringLiteral("RetroAchievements: API rate limit reached\n"));

        if (httpResult.retryAfterUtc.isValid()) {
            output.append(
                QStringLiteral("RetroAchievements: Retry after %1\n")
                    .arg(httpResult.retryAfterUtc.toString(Qt::ISODate)));
        }

        return;
    }

    if (httpResult.status != RaRequestStatus::Success) {
        output.append(QStringLiteral("RetroAchievements: Request failed "
                                     "(HTTP %1): %2\n")
                          .arg(httpResult.httpStatus)
                          .arg(httpResult.errorMessage));
        return;
    }

    // Interpret the response using rcheevos.
    rc_api_resolve_hash_response_t response{};

    const int parseResult = rc_api_process_resolve_hash_response(
        &response, httpResult.body.constData());

    if (parseResult != RC_OK || !response.response.succeeded) {
        const QString error =
            parseResult != RC_OK
                ? QString::fromUtf8(rc_error_str(parseResult))
                : QString::fromUtf8(response.response.error_message
                                        ? response.response.error_message
                                        : "Unknown API error");

        output.append(
            QStringLiteral("RetroAchievements: Identification failed: %1\n")
                .arg(error));

        rc_api_destroy_resolve_hash_response(&response);
        return;
    }

    const std::uint32_t gameId = response.game_id;

    rc_api_destroy_resolve_hash_response(&response);

    if (gameId == 0) {
        output.append(
            QStringLiteral("RetroAchievements: No matching game found\n"));
        return;
    }

    GameEntry entry;
    entry.title = getCompareTitle(info);
    entry.platform = config->platform;
    entry.raHash = hash;
    entry.raGameId = QString::number(gameId);
    entry.cacheResourceTypes = {"rahash", "ragameid"};

    gameEntries.append(entry);
}
