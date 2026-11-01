#include "CareerStorage.hpp"
#include "Game.hpp"

#include <fstream>
#include <random>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fu {
namespace {
namespace fs = std::filesystem;

std::string displayPath(const fs::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

fs::path backupPath(const fs::path& path) {
    fs::path result = path;
    result += ".bak";
    return result;
}

// Inspect the links themselves: exists() alone would miss dangling symlinks.
bool regularOrMissing(const fs::path& path) {
    std::error_code error;
    const auto state = fs::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) return false;
    if (error) throw fs::filesystem_error("Не удалось проверить файл", path, error);
    if (state.type() == fs::file_type::not_found) return false;
    if (!fs::is_regular_file(state))
        throw std::runtime_error("Путь сохранения занят каталогом, ссылкой или специальным файлом");
    return true;
}

void checkParents(const fs::path& path) {
    fs::path current;
    for (const auto& part : fs::absolute(path).parent_path()) {
        current /= part;
        std::error_code error;
        const auto state = fs::symlink_status(current, error);
        if (error == std::errc::no_such_file_or_directory ||
            state.type() == fs::file_type::not_found) continue;
        if (error) throw fs::filesystem_error("Не удалось проверить папку", current, error);
        if (!fs::is_directory(state))
            throw std::runtime_error("Родительский путь не является обычной папкой (ссылки не поддерживаются)");
    }
}

void checkSlotPath(const fs::path& path) {
    if (path.empty() || path.filename().empty() || path.filename() == "." || path.filename() == "..")
        throw std::runtime_error("Не указано имя файла сохранения");
    checkParents(path);
    regularOrMissing(path);
    regularOrMissing(backupPath(path));
}

bool saveSucceeded(const std::string& result) {
    return result.rfind("Карьера сохранена:", 0) == 0;
}

bool loadSucceeded(const std::string& result) {
    return result.rfind("Карьера загружена:", 0) == 0;
}

// Game currently takes narrow paths. A Windows short alias also allows Unicode
// directories when 8.3 aliases are available, without changing process cwd.
std::string gamePath(const fs::path& path) {
#ifdef _WIN32
    auto convert = [](const std::wstring& wide, std::string& result) {
        BOOL substituted = FALSE;
        const bool utf8 = GetACP() == CP_UTF8;
        const DWORD flags = utf8 ? 0 : WC_NO_BEST_FIT_CHARS;
        const int size = WideCharToMultiByte(CP_ACP, flags,
            wide.data(), int(wide.size()), nullptr, 0, nullptr, utf8 ? nullptr : &substituted);
        if (!size || substituted) return false;
        result.resize(size);
        substituted = FALSE;
        return WideCharToMultiByte(CP_ACP, flags,
            wide.data(), int(wide.size()), result.data(), size, nullptr, utf8 ? nullptr : &substituted) != 0 && !substituted;
    };
    std::string result;
    if (convert(path.wstring(), result)) return result;
    const DWORD size = GetShortPathNameW(path.c_str(), nullptr, 0);
    if (size) {
        std::wstring shortPath(size, L'\0');
        const DWORD written = GetShortPathNameW(path.c_str(), shortPath.data(), size);
        if (written && written < size) {
            shortPath.resize(written);
            if (convert(shortPath, result)) return result;
        }
    }
    throw std::runtime_error("Путь недоступен текущей кодировке Windows; выберите папку с поддерживаемым именем");
#else
    return path.string();
#endif
}

class TemporaryFile {
public:
    explicit TemporaryFile(const fs::path& slot) {
        std::random_device random;
        for (int attempt = 0; attempt < 64; ++attempt) {
            path_ = slot;
            path_ += ".tmp." + std::to_string(random()) + "." + std::to_string(random());
#ifdef _WIN32
            HANDLE file = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                if (CloseHandle(file)) return;
                std::error_code ignored;
                fs::remove(path_, ignored);
                throw std::runtime_error("Не удалось закрыть временный файл");
            }
            if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
                throw fs::filesystem_error("Не удалось создать временный файл", path_,
                    std::error_code(GetLastError(), std::system_category()));
#else
            const int file = ::open(path_.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
            if (file >= 0) {
                if (::close(file) == 0) return;
                std::error_code ignored;
                fs::remove(path_, ignored);
                throw std::runtime_error("Не удалось закрыть временный файл");
            }
            if (errno != EEXIST)
                throw fs::filesystem_error("Не удалось создать временный файл", path_,
                    std::error_code(errno, std::generic_category()));
#endif
        }
        throw std::runtime_error("Не удалось подобрать имя временного файла");
    }
    ~TemporaryFile() {
        if (!path_.empty()) {
            std::error_code ignored;
            fs::remove(path_, ignored);
        }
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    const fs::path& path() const { return path_; }
    void release() { path_.clear(); }
private:
    fs::path path_;
};

void flushFile(const fs::path& path) {
#ifdef _WIN32
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw fs::filesystem_error("Не удалось открыть файл для проверки записи", path,
            std::error_code(GetLastError(), std::system_category()));
    const bool flushed = FlushFileBuffers(file) != 0;
    const DWORD error = GetLastError();
    const bool closed = CloseHandle(file) != 0;
    if (!flushed || !closed)
        throw fs::filesystem_error("Не удалось завершить запись файла", path,
            std::error_code(error, std::system_category()));
#else
    const int file = ::open(path.c_str(), O_WRONLY);
    if (file < 0)
        throw fs::filesystem_error("Не удалось открыть файл для проверки записи", path,
            std::error_code(errno, std::generic_category()));
    const int flushed = ::fsync(file);
    const int error = errno;
    const int closed = ::close(file);
    if (flushed != 0 || closed != 0)
        throw fs::filesystem_error("Не удалось завершить запись файла", path,
            std::error_code(flushed != 0 ? error : errno, std::generic_category()));
#endif
}

void replaceFile(const fs::path& from, const fs::path& to) {
    regularOrMissing(to);
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw fs::filesystem_error("Не удалось заменить сохранение", from, to,
            std::error_code(GetLastError(), std::system_category()));
#else
    fs::rename(from, to);
#endif
}

bool validSave(const fs::path& path) {
    std::ifstream readable(path, std::ios::binary);
    if (!readable) throw std::runtime_error("Файл сохранения недоступен для чтения");
    readable.close();
    Game verification;
    return loadSucceeded(verification.load(gamePath(path)));
}

std::string describeFile(const fs::path& path) {
    if (!regularOrMissing(path)) return "пусто";
    Game saved;
    if (!loadSucceeded(saved.load(gamePath(path)))) return "повреждённое или несовместимое сохранение";
    return saved.dateString() + " | " + saved.myClub().name + " | Менеджер: " + saved.manager();
}

} // namespace

std::string saveCareerSafely(const Game& game, const fs::path& path) {
    try {
        checkSlotPath(path);
        if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
        TemporaryFile fresh(path);
        const auto result = game.save(gamePath(fresh.path()));
        if (!saveSucceeded(result)) return "Не удалось сохранить карьеру: " + result;
        flushFile(fresh.path());
        if (!validSave(fresh.path())) return "Не удалось сохранить карьеру: проверка записанного файла не пройдена";

        // Prepare an independent copy before changing either permanent file.
        // An unreadable/corrupt primary must never displace a working backup.
        if (regularOrMissing(path) && validSave(path)) {
            TemporaryFile previous(path);
            fs::copy_file(path, previous.path(), fs::copy_options::overwrite_existing);
            flushFile(previous.path());
            if (!validSave(previous.path()))
                return "Не удалось сохранить карьеру: предыдущий файл изменился во время копирования";
            replaceFile(fresh.path(), path);
            fresh.release();
            try {
                replaceFile(previous.path(), backupPath(path));
                previous.release();
            } catch (const std::exception&) {
                // The old backup is untouched when replacement fails. Restore
                // the old primary too, instead of reporting a half-completed save.
                try {
                    replaceFile(previous.path(), path);
                    previous.release();
                } catch (const std::exception&) {
                    // Retain a recoverable file if the filesystem starts failing
                    // during rollback; losing data is worse than retaining it.
                    const auto recovery = previous.path();
                    previous.release();
                    return "Ошибка сохранения и отката; предыдущая карьера сохранена отдельно: " + displayPath(recovery);
                }
                return "Не удалось обновить резервную копию; основной файл и резервная копия сохранены без изменений";
            }
        } else {
            replaceFile(fresh.path(), path);
            fresh.release();
        }
        return "Карьера сохранена: " + displayPath(path);
    } catch (const std::exception& error) {
        return std::string("Не удалось сохранить карьеру: ") + error.what();
    }
}

std::string loadCareerBackup(Game& game, const fs::path& path) {
    try {
        if (path.empty()) return "Не указан путь сохранения для загрузки резервной копии";
        const auto backup = backupPath(path);
        checkParents(backup);
        if (!regularOrMissing(backup)) return "Резервная копия не найдена: " + displayPath(backup);
        Game restored;
        const auto result = restored.load(gamePath(backup));
        if (!loadSucceeded(result)) return "Не удалось загрузить резервную копию: " + result;
        game = std::move(restored);
        return "Карьера загружена из резервной копии: " + displayPath(backup);
    } catch (const std::exception& error) {
        return std::string("Не удалось загрузить резервную копию: ") + error.what();
    }
}

std::string careerSlotSummary(const fs::path& path) {
    try {
        if (path.empty()) return "Слот не выбран";
        checkParents(path);
        return "Файл: " + displayPath(path) + "\n" + describeFile(path) +
            "\nРезервная копия: " + describeFile(backupPath(path));
    } catch (const std::exception& error) {
        return std::string("Слот недоступен: ") + error.what();
    }
}

} // namespace fu
