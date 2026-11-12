
#include "retroachievementsromhasher.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <archive.h>
#include <archive_entry.h>
#include <cstdint>
#include <memory>
#include <rc_consoles.h>
#include <rc_hash.h>

namespace {

    constexpr qint64 MAX_ROM_SIZE = 256LL * 1024 * 1024;
    constexpr int MAX_ARCHIVE_ENTRIES = 4096;

    QSet<QString> getRomExtensions(std::uint32_t consoleId) {
        switch (consoleId) {
        case RC_CONSOLE_NINTENDO:
            return {"nes"};

        case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
            return {"fds", "nes"};

        case RC_CONSOLE_SUPER_NINTENDO:
            return {"sfc", "smc", "swc", "fig"};

        case RC_CONSOLE_GAMEBOY:
            return {"gb"};

        case RC_CONSOLE_GAMEBOY_COLOR:
            return {"gbc", "gb"};

        case RC_CONSOLE_GAMEBOY_ADVANCE:
            return {"gba", "agb"};

        case RC_CONSOLE_NINTENDO_64:
            return {"z64", "n64", "v64"};

        case RC_CONSOLE_MEGA_DRIVE:
            return {"md", "gen", "bin", "smd"};

        case RC_CONSOLE_SEGA_32X:
            return {"32x", "bin"};

        default:
            return {};
        }
    }

    bool extractRom(const QString &archivePath, std::uint32_t consoleId,
                    QByteArray &romData, QByteArray &romFileName) {
        using ArchivePtr =
            std::unique_ptr<struct archive, decltype(&archive_read_free)>;

        ArchivePtr reader(archive_read_new(), archive_read_free);

        if (!reader) {
            qWarning() << "RetroAchievements: Cannot create archive reader";
            return false;
        }

        archive_read_support_filter_all(reader.get());
        archive_read_support_format_zip(reader.get());
        archive_read_support_format_7zip(reader.get());

        const QByteArray path = QFile::encodeName(archivePath);

        if (archive_read_open_filename(reader.get(), path.constData(), 10240) !=
            ARCHIVE_OK) {
            qWarning() << "RetroAchievements: Cannot open archive:"
                       << archive_error_string(reader.get());
            return false;
        }

        const QSet<QString> extensions = getRomExtensions(consoleId);

        struct archive_entry *entry = nullptr;

        bool found = false;
        int entryCount = 0;
        int result = ARCHIVE_OK;

        while ((result = archive_read_next_header(reader.get(), &entry)) ==
               ARCHIVE_OK) {
            if (++entryCount > MAX_ARCHIVE_ENTRIES) {
                qWarning() << "RetroAchievements: Too many archive entries:"
                           << archivePath;
                return false;
            }

            if (archive_entry_filetype(entry) != AE_IFREG) {
                continue;
            }

            const char *entryPath = archive_entry_pathname_utf8(entry);

            if (!entryPath) {
                entryPath = archive_entry_pathname(entry);
            }

            if (!entryPath) {
                continue;
            }

            QString entryName = QString::fromUtf8(entryPath);
            entryName.replace('\\', '/');

            if (entryName.startsWith("__MACOSX/", Qt::CaseInsensitive)) {
                continue;
            }

            const QFileInfo entryInfo(entryName);
            const QString fileName = entryInfo.fileName();

            if (fileName.startsWith("._") ||
                !extensions.contains(entryInfo.suffix().toLower())) {
                continue;
            }

            // Avoid silently selecting the wrong ROM from an archive.
            if (found) {
                qWarning()
                    << "RetroAchievements: Multiple ROMs found in archive:"
                    << archivePath;
                return false;
            }

            const la_int64_t declaredSize = archive_entry_size(entry);

            if (declaredSize > MAX_ROM_SIZE) {
                qWarning()
                    << "RetroAchievements: ROM exceeds maximum supported size:"
                    << fileName;
                return false;
            }

            romFileName = fileName.toUtf8();

            char buffer[64 * 1024];

            while (true) {
                const la_ssize_t bytesRead =
                    archive_read_data(reader.get(), buffer, sizeof(buffer));

                if (bytesRead == 0) {
                    break;
                }

                if (bytesRead < 0) {
                    qWarning() << "RetroAchievements: Failed to extract ROM:"
                               << archive_error_string(reader.get());
                    return false;
                }

                if (bytesRead > MAX_ROM_SIZE - romData.size()) {
                    qWarning()
                        << "RetroAchievements: Extracted ROM is too large:"
                        << fileName;
                    return false;
                }

                romData.append(buffer, static_cast<int>(bytesRead));
            }

            if (romData.isEmpty()) {
                qWarning() << "RetroAchievements: Extracted ROM is empty:"
                           << fileName;
                return false;
            }

            found = true;
        }

        if (result != ARCHIVE_EOF) {
            qWarning() << "RetroAchievements: Archive processing failed:"
                       << archive_error_string(reader.get());
            return false;
        }

        if (!found) {
            qWarning() << "RetroAchievements: No supported ROM found in:"
                       << archivePath;
        }

        return found;
    }

} // namespace

QString RetroAchievementsRomHasher::generate(const QString &romPath,
                                             std::uint32_t consoleId) {
    if (consoleId == RC_CONSOLE_UNKNOWN) {
        return {};
    }

    const QFileInfo info(romPath);

    if (!info.isFile() || !info.isReadable()) {
        return {};
    }

    const QString extension = info.suffix().toLower();

    const bool isArchive = extension == "zip" || extension == "7z";

    QByteArray romData;
    QByteArray romFileName;

    if (isArchive) {
        if (!extractRom(info.absoluteFilePath(), consoleId, romData,
                        romFileName)) {
            return {};
        }
    } else {
        romFileName = QFile::encodeName(info.absoluteFilePath());
    }

    rc_hash_iterator_t iterator{};

    const auto *buffer =
        isArchive ? reinterpret_cast<const std::uint8_t *>(romData.constData())
                  : nullptr;

    rc_hash_initialize_iterator(
        &iterator, romFileName.constData(), buffer,
        isArchive ? static_cast<std::size_t>(romData.size()) : 0);

    char hash[33]{};

    const int success = rc_hash_generate(hash, consoleId, &iterator);

    rc_hash_destroy_iterator(&iterator);

    if (!success) {
        qWarning() << "RetroAchievements: Failed to generate hash for:"
                   << romPath;
        return {};
    }

    return QString::fromLatin1(hash);
}

std::uint32_t
RetroAchievementsRomHasher::consoleIdForPlatform(const QString &platform) {
    static const QHash<QString, std::uint32_t> consoleIds = {
        {"fds", RC_CONSOLE_FAMICOM_DISK_SYSTEM},
        {"nes", RC_CONSOLE_NINTENDO},
        {"snes", RC_CONSOLE_SUPER_NINTENDO},
        {"gb", RC_CONSOLE_GAMEBOY},
        {"gbc", RC_CONSOLE_GAMEBOY_COLOR},
        {"gba", RC_CONSOLE_GAMEBOY_ADVANCE},
        {"n64", RC_CONSOLE_NINTENDO_64},
        {"megadrive", RC_CONSOLE_MEGA_DRIVE},
        {"genesis", RC_CONSOLE_MEGA_DRIVE},
        {"sega32x", RC_CONSOLE_SEGA_32X}};

    return consoleIds.value(platform.trimmed().toLower(), RC_CONSOLE_UNKNOWN);
}
