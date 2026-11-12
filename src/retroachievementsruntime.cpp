#include "retroachievementsruntime.h"

#include "config.h"
#include "retroachievementsrequestscheduler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMetaObject>
#include <QThread>

RetroAchievementsRuntime &RetroAchievementsRuntime::instance() {
    static RetroAchievementsRuntime runtime;
    return runtime;
}

RetroAchievementsRuntime::RetroAchievementsRuntime() {
    QCoreApplication *app = QCoreApplication::instance();

    if (!app) {
        qFatal("RetroAchievements requires a QCoreApplication");
    }

    if (QThread::currentThread() == app->thread()) {
        initialize();
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        app, [this]() { initialize(); }, Qt::BlockingQueuedConnection);

    if (!invoked) {
        qFatal("Failed to initialize RetroAchievements runtime");
    }
}

RetroAchievementsRuntime::~RetroAchievementsRuntime() { shutdown(); }

void RetroAchievementsRuntime::initialize() {
    QCoreApplication *app = QCoreApplication::instance();

    Q_ASSERT(app != nullptr);
    Q_ASSERT(QThread::currentThread() == app->thread());

    const QString statePath =
        QDir(Config::getSkyFolder(Config::SkyFolderType::LOG))
            .filePath("retroachievements-rate-limit.ini");

    thread_ = new QThread;
    thread_->setObjectName("RetroAchievementsRequests");

    scheduler_ = new RetroAchievementsRequestScheduler(statePath);
    scheduler_->moveToThread(thread_);

    QObject::connect(thread_, &QThread::started, scheduler_,
                     &RetroAchievementsRequestScheduler::initialize,
                     Qt::DirectConnection);

    QObject::connect(thread_, &QThread::finished, scheduler_,
                     &QObject::deleteLater);

    QObject::connect(app, &QCoreApplication::aboutToQuit, app,
                     [this]() { shutdown(); });

    thread_->start();
}

RetroAchievementsRequestScheduler *RetroAchievementsRuntime::scheduler() const {
    return scheduler_;
}

void RetroAchievementsRuntime::shutdown() {
    if (!thread_) {
        return;
    }

    if (thread_->isRunning()) {
        const bool invoked = QMetaObject::invokeMethod(
            scheduler_, "shutdown", Qt::BlockingQueuedConnection);

        if (!invoked) {
            qWarning() << "Failed to shut down RetroAchievements scheduler";
        }

        thread_->quit();
        thread_->wait();
    }

    delete thread_;

    thread_ = nullptr;
    scheduler_ = nullptr;
}