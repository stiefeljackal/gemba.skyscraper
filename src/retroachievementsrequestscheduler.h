
#ifndef RETROACHIEVEMENTSREQUESTSCHEDULER_H
#define RETROACHIEVEMENTSREQUESTSCHEDULER_H

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

enum class RaRequestStatus {
    Success,
    RateLimited,
    NetworkError,
    HttpError,
    Cancelled,
    Rejected
};

struct RaHttpResult {
    RaRequestStatus status = RaRequestStatus::Rejected;
    int httpStatus = 0;
    QByteArray body;
    QString errorMessage;
    QDateTime retryAfterUtc;
};

Q_DECLARE_METATYPE(RaHttpResult)

class RetroAchievementsRequestScheduler final : public QObject {
    Q_OBJECT

public:
    struct Policy {
        int intervalMs = 5000;
        int timeoutMs = 30000;
        int maxRetries = 2;
        int fallbackCooldownSeconds = 300;
        int maxPendingHashes = 64;
    };

    explicit RetroAchievementsRequestScheduler(const QString &stateFilePath,
                                               QObject *parent = nullptr);

    RetroAchievementsRequestScheduler(const QString &stateFilePath,
                                      const Policy &policy,
                                      QObject *parent = nullptr);

public slots:
    void initialize();

    void enqueue(quint64 requestId, const QString &romHash, const QString &url,
                 const QByteArray &postData);

    void cancel(quint64 requestId);
    void shutdown();

signals:
    void requestFinished(quint64 requestId, const RaHttpResult &result);

private:
    struct PendingRequest {
        QString url;
        QByteArray postData;
        QList<quint64> subscribers;
        int retries = 0;
    };

    void processQueue();
    void handleReply(QNetworkReply *reply);

    void completeRequest(const QString &hash, const RaHttpResult &result);

    void completeAll(const RaHttpResult &result);

    QDateTime parseRetryAfter(const QByteArray &header) const;

    void persistCooldown(const QDateTime &untilUtc);

    QString userAgent() const;

    QString stateFilePath_;
    Policy policy_;

    QNetworkAccessManager *networkManager_ = nullptr;
    QTimer *requestTimer_ = nullptr;
    QNetworkReply *activeReply_ = nullptr;

    QQueue<QString> requestQueue_;
    QHash<QString, PendingRequest> pendingRequests_;

    QString activeHash_;

    QElapsedTimer lastRequestStart_;
    QDateTime retryNotBeforeUtc_;
    QDateTime cooldownUntilUtc_;

    bool rateLimited_ = false;
    bool shuttingDown_ = false;
};

#endif // RETROACHIEVEMENTSREQUESTSCHEDULER_H
