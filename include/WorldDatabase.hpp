#pragma once
#include <cstddef>
#include <string>
#include <optional>
#include <unordered_map>
#include <vector>

namespace fu {
// Read-only OFFLINE football identity catalogue. Not the 72-club simulation roster.
// In particular, a catalogue entry without verified current club information is
// NOT treated as a free agent or a transfer candidate.
struct WorldPlayer {
    std::string id, wikidataQid, name, position, birthDate, nationality;
    int age=0; // 0 = birth date not known; base year 2026 for snapshot.
};
struct WorldQuery {
    std::string name, position, country;
    int maxAge=0; // 0 = don't filter. Rows without DOB are excluded if filtering.
    size_t offset=0, limit=120;
};
struct WorldResults {
    size_t total=0;
    std::vector<size_t> indices;
};
class WorldDatabase {
public:
    bool load(const std::string& path);
    size_t size() const { return players_.size(); }
    const WorldPlayer* at(size_t index) const;
    const std::string& error() const { return error_; }
    WorldResults search(const WorldQuery& query) const;
    std::optional<size_t> indexById(const std::string& id) const;
    WorldResults searchWatched(const WorldQuery& query,
                               const std::vector<std::string>& watchIds) const;
private:
    std::vector<WorldPlayer> players_;
    std::unordered_map<std::string,size_t> idLookup_;
    std::string error_;
};
}
