#include "CareerStorage.hpp"
#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

struct TestDirectory {
    fs::path path;
    TestDirectory() {
        std::random_device random;
        do {
            path = fs::temp_directory_path() /
                ("fu-career-storage-test-" + std::to_string(random()) + "-" + std::to_string(random()));
        } while (!fs::create_directory(path));
    }
    ~TestDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    output.close();
    assert(output);
}

bool saved(const std::string& result) {
    return result.rfind("Карьера сохранена:", 0) == 0;
}

void noTemporaryFiles(const fs::path& directory) {
    for (const auto& entry : fs::recursive_directory_iterator(directory))
        assert(entry.path().filename().string().find(".tmp.") == std::string::npos);
}
} // namespace

int main() {
    TestDirectory temporary;
    const auto slot = temporary.path / "slots" / "slot1.fus";
    const auto backup = fs::path(slot.string() + ".bak");
    fu::Game game;
    game.newCareer("Первый тренер", 0, fu::Role::Coach, fu::Difficulty::Easy, {0, 1});
    const auto originalStatus = game.status();
    assert(saved(fu::saveCareerSafely(game, slot)));
    assert(game.status() == originalStatus);
    assert(!fs::exists(backup));
    const auto first = bytes(slot);

    game.newCareer("Второй тренер", 6, fu::Role::Coach, fu::Difficulty::Realistic, {0, 1});
    game.advance(2);
    assert(saved(fu::saveCareerSafely(game, slot)));
    const auto second = bytes(slot);
    assert(second != first);
    assert(bytes(backup) == first);

    game.newCareer("Третий тренер", 12, fu::Role::Coach, fu::Difficulty::Hard, {0, 2});
    game.advance(3);
    assert(saved(fu::saveCareerSafely(game, slot)));
    const auto third = bytes(slot);
    assert(bytes(backup) == second);
    fu::Game loaded;
    assert(loaded.load(slot.string()).find("Карьера загружена:") == 0);
    assert(loaded.manager() == game.manager());
    assert(loaded.teamId() == game.teamId());
    assert(loaded.dateString() == game.dateString());
    auto summary = fu::careerSlotSummary(slot);
    assert(summary.find("Третий тренер") != std::string::npos);
    assert(summary.find(game.myClub().name) != std::string::npos);
    assert(summary.find(game.dateString()) != std::string::npos);
    assert(summary.find("Второй тренер") != std::string::npos);
    assert(bytes(slot) == third && bytes(backup) == second); // Summary is read-only.

    assert(fu::loadCareerBackup(loaded, slot).find("Карьера загружена из резервной копии:") == 0);
    assert(loaded.manager() == "Второй тренер");
    assert(loaded.teamId() == 6);
    assert(loaded.day() == 2);
    assert(bytes(slot) == third && bytes(backup) == second);

    // A corrupt primary must never replace a known-good backup.
    write(slot, "FUSAVE_12\ntruncated\n");
    summary = fu::careerSlotSummary(slot);
    assert(summary.find("повреждённое") != std::string::npos);
    assert(summary.find("Второй тренер") != std::string::npos);
    assert(saved(fu::saveCareerSafely(game, slot)));
    assert(bytes(backup) == second);

    // Failed backup loads leave all live state (including status and RNG) intact.
    const auto stateBefore = temporary.path / "state-before.fus";
    const auto stateAfter = temporary.path / "state-after.fus";
    assert(saved(loaded.save(stateBefore.string())));
    write(backup, "not a career");
    assert(fu::loadCareerBackup(loaded, slot).find("Не удалось") == 0);
    assert(saved(loaded.save(stateAfter.string())));
    assert(bytes(stateBefore) == bytes(stateAfter));
    fs::remove(backup);
    assert(fu::loadCareerBackup(loaded, slot).find("Резервная копия не найдена:") == 0);
    assert(saved(loaded.save(stateAfter.string())));
    assert(bytes(stateBefore) == bytes(stateAfter));

    // Neither an invalid in-memory career nor a backup name collision changes
    // existing permanent files or leaves a partly written temporary save.
    write(backup, second);
    const auto primaryBeforeFailure = bytes(slot);
    game.myClub().captainId = 99999999;
    assert(!saved(fu::saveCareerSafely(game, slot)));
    assert(bytes(slot) == primaryBeforeFailure && bytes(backup) == second);
    game.myClub().captainId = -1;
    fs::remove(backup);
    fs::create_directory(backup);
    write(backup / "keep.txt", "untouched");
    assert(!saved(fu::saveCareerSafely(game, slot)));
    assert(bytes(slot) == primaryBeforeFailure);
    assert(bytes(backup / "keep.txt") == "untouched");
    const auto directoryTarget = temporary.path / "directory.fus";
    fs::create_directory(directoryTarget);
    assert(!saved(fu::saveCareerSafely(game, directoryTarget)));
    const auto parentFile = temporary.path / "parent-is-file";
    write(parentFile, "untouched parent");
    assert(!saved(fu::saveCareerSafely(game, parentFile / "slot.fus")));
    assert(bytes(parentFile) == "untouched parent");
    assert(!saved(fu::saveCareerSafely(game, fs::path{})));

#ifndef _WIN32
    const auto external = temporary.path / "external.fus";
    write(external, "untouched target");
    const auto linked = temporary.path / "linked.fus";
    fs::create_symlink(external, linked);
    assert(!saved(fu::saveCareerSafely(game, linked)));
    assert(bytes(external) == "untouched target" && fs::is_symlink(linked));
    const auto linkedParent = temporary.path / "linked-parent";
    fs::create_directory_symlink(slot.parent_path(), linkedParent);
    assert(!saved(fu::saveCareerSafely(game, linkedParent / "slot1.fus")));
    assert(bytes(slot) == primaryBeforeFailure);
    const auto backupLinkSlot = temporary.path / "backup-link.fus";
    write(backupLinkSlot, primaryBeforeFailure);
    fs::create_symlink(external, fs::path(backupLinkSlot.string() + ".bak"));
    assert(!saved(fu::saveCareerSafely(game, backupLinkSlot)));
    assert(bytes(backupLinkSlot) == primaryBeforeFailure);
    assert(bytes(external) == "untouched target");

    // Root intentionally bypasses POSIX permissions; exercise this failure
    // only when directory permissions can actually restrict the current user.
    if (::geteuid() != 0) {
        const auto protectedSlot = temporary.path / "protected" / "career.fus";
        assert(saved(fu::saveCareerSafely(game, protectedSlot)));
        assert(saved(fu::saveCareerSafely(game, protectedSlot)));
        const auto protectedBackup = fs::path(protectedSlot.string() + ".bak");
        const auto primaryBytes = bytes(protectedSlot);
        const auto backupBytes = bytes(protectedBackup);
        const auto originalPermissions = fs::status(protectedSlot.parent_path()).permissions();
        fs::permissions(protectedSlot.parent_path(), fs::perms::owner_read | fs::perms::owner_exec);
        const auto failedResult = fu::saveCareerSafely(game, protectedSlot);
        fs::permissions(protectedSlot.parent_path(), originalPermissions);
        assert(!saved(failedResult));
        assert(bytes(protectedSlot) == primaryBytes);
        assert(bytes(protectedBackup) == backupBytes);
    }
#endif

    assert(fu::careerSlotSummary(temporary.path / "empty.fus").find("пусто") != std::string::npos);
    noTemporaryFiles(temporary.path);
    std::cout << "PASS: safe save rotation, validation, explicit backup restore, slot summaries and failed-write preservation\n";
}
