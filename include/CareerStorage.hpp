#pragma once

#include <filesystem>
#include <string>

namespace fu {

class Game;

// A slot's .bak contains its previous valid save. Loading a backup is explicit.
std::string saveCareerSafely(const Game& game, const std::filesystem::path& path);
std::string loadCareerBackup(Game& game, const std::filesystem::path& path);
std::string careerSlotSummary(const std::filesystem::path& path);

} // namespace fu
