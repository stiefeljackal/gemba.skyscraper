
#ifndef RETROACHIEVEMENTSROMHASHER_H
#define RETROACHIEVEMENTSROMHASHER_H

#include <QString>
#include <cstdint>

class RetroAchievementsRomHasher final
{
public:
    static QString generate(const QString &romPath, std::uint32_t consoleId);
    static std::uint32_t consoleIdForPlatform(const QString &platform);
};

#endif // RETROACHIEVEMENTSROMHASHER_H
