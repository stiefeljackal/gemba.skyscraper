
#include "retroachievementsromhasher.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <rc_hash.h>
#include <rc_consoles.h>

QString RetroAchievementsRomHasher::generate(const QString &romPath, std::uint32_t consoleId)
{
    if (consoleId == RC_CONSOLE_UNKNOWN)
        return {};

    const QFileInfo info(romPath);

    if (!info.isFile() || !info.isReadable())
        return {};

    const QByteArray path = QFile::encodeName(info.absoluteFilePath());

    rc_hash_iterator_t iterator = {};

    rc_hash_initialize_iterator(&iterator, path.constData(), nullptr, 0);

    char hash[33] = {};

    const int success = rc_hash_generate(hash, consoleId, &iterator);

    rc_hash_destroy_iterator(&iterator);

    if (!success)
        return {};

    return QString::fromLatin1(hash);
}

std::uint32_t RetroAchievementsRomHasher::consoleIdForPlatform(const QString &platform)
{
    static const QHash<QString, std::uint32_t> consoleIds = {
        {QStringLiteral("nes"), RC_CONSOLE_NINTENDO},
        {QStringLiteral("snes"), RC_CONSOLE_SUPER_NINTENDO},
        {QStringLiteral("gb"), RC_CONSOLE_GAMEBOY},
        {QStringLiteral("gbc"), RC_CONSOLE_GAMEBOY_COLOR},
        {QStringLiteral("gba"), RC_CONSOLE_GAMEBOY_ADVANCE},
        {QStringLiteral("n64"), RC_CONSOLE_NINTENDO_64},
        {QStringLiteral("megadrive"), RC_CONSOLE_MEGA_DRIVE},
        {QStringLiteral("genesis"), RC_CONSOLE_MEGA_DRIVE},
        {QStringLiteral("sega32x"), RC_CONSOLE_SEGA_32X}
    };

    return consoleIds.value(platform.trimmed().toLower(), RC_CONSOLE_UNKNOWN);
}
