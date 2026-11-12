
#include "retroachievementsrequestscheduler.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <limits>

namespace {

    bool isTransientHttpError(int status) {
        switch (status) {
        case 408:
        case 500:
        case 502:
        case 503:
        case 504:
            return true;

        default:
            return false;
        }
    }

    bool isTransientNetworkError(QNetworkReply::NetworkError error) {
        switch (error) {
        case QNetworkReply::TimeoutError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::NetworkSessionFailedError:
            return true;

        default:
            return false;
        }
    }

    int boundedDelay(qint64 milliseconds) {
        return static_cast<int>(
            std::min<qint64>(std::max<qint64>(milliseconds, 0),
                             std::numeric_limits<int>::max()));
    }

} // namespace

RetroAchievementsRequestScheduler::RetroAchievementsRequestScheduler(
    const QString &stateFilePath, QObject *parent)
    : RetroAchievementsRequestScheduler(stateFilePath, Policy{}, parent) {}

RetroAchievementsRequestScheduler::RetroAchievementsRequestScheduler(
    const QString &stateFilePath, const Policy &policy, QObject *parent)
    : QObject(parent), stateFilePath_(stateFilePath), policy_(policy) {

    qRegisterMetaType<RaHttpResult>("RaHttpResult");
}

void RetroAchievementsRequestScheduler::initialize() {
    Q_ASSERT(thread() == QThread::currentThread());

    if (networkManager_ || shuttingDown_)
        return;

    networkManager_ = new QNetworkAccessManager(this);

    requestTimer_ = new QTimer(this);
    requestTimer_->setSingleShot(true);

    connect(requestTimer_, &QTimer::timeout, this,
            &RetroAchievementsRequestScheduler::processQueue);

    QSettings settings(stateFilePath_, QSettings::IniFormat);

    cooldownUntilUtc_ =
        QDateTime::fromString(settings.value("rateLimitUntilUtc").toString(),
                              Qt::ISODateWithMs)
            .toUTC();

    if (cooldownUntilUtc_.isValid() &&
        cooldownUntilUtc_ <= QDateTime::currentDateTimeUtc()) {

        cooldownUntilUtc_ = QDateTime();

        settings.remove("rateLimitUntilUtc");
        settings.sync();
    }
}

void RetroAchievementsRequestScheduler::enqueue(quint64 requestId,
                                                const QString &romHash,
                                                const QString &url,
                                                const QByteArray &postData) {

    Q_ASSERT(thread() == QThread::currentThread());

    const QString hash = romHash.trimmed().toLower();
    const QUrl requestUrl(url);

    RaHttpResult rejected;

    if (shuttingDown_ || !networkManager_) {
        rejected.errorMessage = "Scheduler is not running";
    } else if (rateLimited_ ||
               (cooldownUntilUtc_.isValid() &&
                cooldownUntilUtc_ > QDateTime::currentDateTimeUtc())) {

        rejected.status = RaRequestStatus::RateLimited;
        rejected.retryAfterUtc = cooldownUntilUtc_;
        rejected.errorMessage = "RetroAchievements API cooldown is active";

    } else if (hash.isEmpty() || !requestUrl.isValid() ||
               requestUrl.scheme() != "https") {

        rejected.errorMessage = "Invalid ROM hash or HTTPS URL";

    } else if (pendingRequests_.contains(hash)) {
        PendingRequest &pending = pendingRequests_[hash];

        if (pending.url != url || pending.postData != postData) {

            rejected.errorMessage =
                "Conflicting requests for the same ROM hash";
        } else {
            pending.subscribers.append(requestId);
            return;
        }

    } else if (pendingRequests_.size() >=
               std::max(1, policy_.maxPendingHashes)) {

        rejected.errorMessage = "RetroAchievements request queue is full";

    } else {
        PendingRequest pending;

        pending.url = url;
        pending.postData = postData;
        pending.subscribers.append(requestId);

        pendingRequests_.insert(hash, pending);
        requestQueue_.enqueue(hash);

        processQueue();
        return;
    }

    emit requestFinished(requestId, rejected);
}

void RetroAchievementsRequestScheduler::processQueue() {
    Q_ASSERT(thread() == QThread::currentThread());

    if (shuttingDown_ || rateLimited_ || !networkManager_ || activeReply_ ||
        requestQueue_.isEmpty()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (cooldownUntilUtc_.isValid() && now < cooldownUntilUtc_) {
        return;
    }

    const qint64 spacing =
        lastRequestStart_.isValid()
            ? std::max<qint64>(0,
                               policy_.intervalMs - lastRequestStart_.elapsed())
            : 0;

    const qint64 backoff =
        retryNotBeforeUtc_.isValid()
            ? std::max<qint64>(0, now.msecsTo(retryNotBeforeUtc_))
            : 0;

    const qint64 waitMs = std::max(spacing, backoff);

    if (waitMs > 0) {
        requestTimer_->start(boundedDelay(waitMs));
        return;
    }

    while (!requestQueue_.isEmpty()) {
        const QString hash = requestQueue_.dequeue();

        if (!pendingRequests_.contains(hash))
            continue;

        const PendingRequest pending = pendingRequests_.value(hash);

        QNetworkRequest request{QUrl(pending.url)};

        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());

        activeHash_ = hash;

        if (pending.postData.isNull()) {
            activeReply_ = networkManager_->get(request);
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              "application/x-www-form-urlencoded");

            activeReply_ = networkManager_->post(request, pending.postData);
        }

        lastRequestStart_.start();
        retryNotBeforeUtc_ = QDateTime();

        QNetworkReply *reply = activeReply_;

        connect(reply, &QNetworkReply::finished, this,
                [this, reply]() { handleReply(reply); });

        // An absolute request deadline, including stalled responses.
        QTimer::singleShot(std::max(1000, policy_.timeoutMs), reply, [reply]() {
            if (reply->isRunning()) {
                reply->setProperty("raTimedOut", true);
                reply->abort();
            }
        });

        return;
    }
}

void RetroAchievementsRequestScheduler::handleReply(QNetworkReply *reply) {

    Q_ASSERT(thread() == QThread::currentThread());

    if (reply != activeReply_)
        return;

    const QString hash = activeHash_;

    activeHash_.clear();
    activeReply_ = nullptr;

    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QByteArray retryAfterHeader = reply->rawHeader("Retry-After");

    const QNetworkReply::NetworkError error = reply->error();

    const bool timedOut = reply->property("raTimedOut").toBool();

    const QString errorString = reply->errorString();
    const QByteArray responseBody = reply->readAll();

    reply->deleteLater();

    if (shuttingDown_ || !pendingRequests_.contains(hash)) {
        processQueue();
        return;
    }

    RaHttpResult result;

    result.httpStatus = httpStatus;
    result.body = responseBody;

    if (httpStatus == 429) {
        result.status = RaRequestStatus::RateLimited;
        result.errorMessage = "HTTP 429 Too Many Requests";

        result.retryAfterUtc = parseRetryAfter(retryAfterHeader);

        persistCooldown(result.retryAfterUtc);

        rateLimited_ = true;

        completeAll(result);
        return;
    }

    if (httpStatus >= 200 && httpStatus < 300 &&
        error == QNetworkReply::NoError) {

        result.status = RaRequestStatus::Success;
        completeRequest(hash, result);

    } else {
        PendingRequest &pending = pendingRequests_[hash];

        const bool retryable =
            isTransientHttpError(httpStatus) || timedOut ||
            (httpStatus == 0 && isTransientNetworkError(error));

        if (retryable && pending.retries < std::max(0, policy_.maxRetries)) {

            ++pending.retries;

            const int baseDelay = pending.retries == 1 ? 5000 : 15000;

            const int jitter = QRandomGenerator::global()->bounded(1000);

            retryNotBeforeUtc_ =
                QDateTime::currentDateTimeUtc().addMSecs(baseDelay + jitter);

            requestQueue_.prepend(hash);

        } else {
            result.status = httpStatus >= 400 ? RaRequestStatus::HttpError
                                              : RaRequestStatus::NetworkError;

            result.errorMessage =
                timedOut ? QStringLiteral("Request timed out") : errorString;

            completeRequest(hash, result);
        }
    }

    processQueue();
}

void RetroAchievementsRequestScheduler::completeRequest(
    const QString &hash, const RaHttpResult &result) {

    auto it = pendingRequests_.find(hash);

    if (it == pendingRequests_.end())
        return;

    const QList<quint64> subscribers = it->subscribers;

    pendingRequests_.erase(it);

    for (quint64 requestId : subscribers) {
        emit requestFinished(requestId, result);
    }
}

void RetroAchievementsRequestScheduler::completeAll(
    const RaHttpResult &result) {

    const QList<QString> hashes = pendingRequests_.keys();

    for (const QString &hash : hashes) {
        completeRequest(hash, result);
    }

    requestQueue_.clear();

    if (requestTimer_)
        requestTimer_->stop();
}

QDateTime RetroAchievementsRequestScheduler::parseRetryAfter(
    const QByteArray &header) const {

    const QByteArray value = header.trimmed();

    bool validNumber = false;

    const qint64 seconds = value.toLongLong(&validNumber);

    if (validNumber && seconds >= 0) {
        return QDateTime::currentDateTimeUtc().addSecs(seconds);
    }

    const QString text = QString::fromLatin1(value);

    QDateTime date = QDateTime::fromString(text, Qt::RFC2822Date);

    if (!date.isValid()) {
        date = QLocale::c().toDateTime(
            text, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
        if (date.isValid()) {
            date.setTimeZone(QTimeZone(QTimeZone::UTC));
        }
    }

    if (date.isValid() && date.toUTC() > QDateTime::currentDateTimeUtc()) {
        return date.toUTC();
    }

    return QDateTime::currentDateTimeUtc().addSecs(
        std::max(1, policy_.fallbackCooldownSeconds));
}

void RetroAchievementsRequestScheduler::persistCooldown(
    const QDateTime &untilUtc) {

    cooldownUntilUtc_ = untilUtc;

    const QString directory = QFileInfo(stateFilePath_).absolutePath();

    if (!QDir().mkpath(directory)) {
        qWarning() << "Cannot create RetroAchievements state directory:"
                   << directory;
        return;
    }

    QSettings settings(stateFilePath_, QSettings::IniFormat);

    settings.setValue("rateLimitUntilUtc",
                      untilUtc.toUTC().toString(Qt::ISODateWithMs));

    settings.sync();

    if (settings.status() != QSettings::NoError) {
        qWarning() << "Failed to persist RetroAchievements cooldown";
    }
}

QString RetroAchievementsRequestScheduler::userAgent() const {
    return QStringLiteral("Skyscraper/%1 (%2; %3; %4)")
        .arg(QString::fromUtf8(VERSION), QSysInfo::kernelType(),
             QSysInfo::productType(), QSysInfo::currentCpuArchitecture());
}

void RetroAchievementsRequestScheduler::cancel(quint64 requestId) {

    Q_ASSERT(thread() == QThread::currentThread());

    for (auto it = pendingRequests_.begin(); it != pendingRequests_.end();
         ++it) {

        if (!it->subscribers.removeOne(requestId))
            continue;

        RaHttpResult cancelled;

        cancelled.status = RaRequestStatus::Cancelled;
        cancelled.errorMessage = "Request cancelled";

        emit requestFinished(requestId, cancelled);

        if (it->subscribers.isEmpty() && it.key() != activeHash_) {

            const QString hash = it.key();

            pendingRequests_.erase(it);
            requestQueue_.removeAll(hash);
        }

        return;
    }
}

void RetroAchievementsRequestScheduler::shutdown() {
    Q_ASSERT(thread() == QThread::currentThread());

    if (shuttingDown_)
        return;

    shuttingDown_ = true;

    if (requestTimer_)
        requestTimer_->stop();

    if (activeReply_) {
        disconnect(activeReply_, nullptr, this, nullptr);

        activeReply_->abort();
        activeReply_->deleteLater();

        activeReply_ = nullptr;
        activeHash_.clear();
    }

    RaHttpResult cancelled;

    cancelled.status = RaRequestStatus::Cancelled;
    cancelled.errorMessage = "Scheduler shutting down";

    completeAll(cancelled);
}
