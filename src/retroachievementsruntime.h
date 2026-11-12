
#ifndef RETROACHIEVEMENTSRUNTIME_H
#define RETROACHIEVEMENTSRUNTIME_H

class QThread;
class RetroAchievementsRequestScheduler;

class RetroAchievementsRuntime final {
public:
    static RetroAchievementsRuntime &instance();

    RetroAchievementsRequestScheduler *scheduler() const;

    RetroAchievementsRuntime(const RetroAchievementsRuntime &) = delete;
    RetroAchievementsRuntime &
    operator=(const RetroAchievementsRuntime &) = delete;

private:
    RetroAchievementsRuntime();
    ~RetroAchievementsRuntime();

    void initialize();
    void shutdown();

    QThread *thread_ = nullptr;
    RetroAchievementsRequestScheduler *scheduler_ = nullptr;
};

#endif // RETROACHIEVEMENTSRUNTIME_H
