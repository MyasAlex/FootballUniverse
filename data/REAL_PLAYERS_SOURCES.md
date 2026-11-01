# Real footballer snapshot — 2026/27 (offline)

This game bundles **280 real-player identities** mapped to 35 of its 36 modeled top-division clubs. The remaining slots and fictional lower divisions are populated by procedural footballers. This is **not a complete 2026/27 official club registration database** and is not refreshed automatically.

## Provenance

- Player names, date of birth, World Cup 2026 participant identities, club name and broad position, and 2026-06 market-value estimates were matched from [ericsanmiguel/football_elo — data/squads/2026.csv](https://github.com/ericsanmiguel/football_elo/blob/main/data/squads/2026.csv). Original project is MIT-licensed, copyright © 2026 Eric San Miguel. See its [LICENSE](https://github.com/ericsanmiguel/football_elo/blob/main/LICENSE).
- `WC:<id>` identifiers in `real_players.csv` correspond to the source `player_id` values. Rows are a subset selected for clubs represented by Football Universe; club spellings and positions are normalized to the game's roster system.
- Market values are a **June 2026 snapshot**, not guaranteed current 2026/27 valuations or transfer fees. They are estimates from the source and may be inaccurate. The game models salaries, abilities, potential and contracts separately and does not claim they are real-life values.
- Some transfers after the snapshot may make club assignments stale. This is a historical starting database, not a live football feed.

## File format

`data/real_players.csv` is UTF-8 and semicolon-delimited:

```csv
club_id;source_id;name;position;birth_date;market_value_eur
0;WC:357662;Declan Rice;MC;1999-01-14;120000000
```

The game imports this file automatically **when starting a new career**. For compatibility, the importer also accepts the old five-column format without market values. Market values are applied as source snapshots; individual in-game attributes are estimated for gameplay, not scraped FIFA/FC or Football Manager ratings.

## Attribution and usage

Source: Eric San Miguel, `football_elo`, MIT License. Copyright © 2026 Eric San Miguel. Permission notice and terms are at the source link above. This repository contains a transformed subset of the data and does not imply endorsement by the source author.

Real player and club names are used descriptively; Football Universe is an unofficial independent prototype with no football federation, club or player endorsement.
