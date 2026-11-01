# Источники реальных футболистов — Football Universe 0.12

**Снимок состава:** 1 ноября 2026 года (указан в `real_players_meta.json`).

## Происхождение

1. **Wikipedia contributors.** Списки действующих футболистов с указанием команд и позиций берутся из разделов текущих составов 36 клубных статей английской Википедии. Условия: [Creative Commons Attribution-ShareAlike 4.0](https://creativecommons.org/licenses/by-sa/4.0/). Статьи и история правок доступны по ссылкам ниже. Учитывайте, что перенос фактов в другую базу и использование в коммерческом продукте могут требовать выполнения условий лицензии.
2. **Wikidata community.** Уникальные идентификаторы спортсменов и даты рождения берутся из Wikidata ([CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)). Адрес отдельного футболиста: `https://www.wikidata.org/wiki/<QID>`.
3. **Football Universe.** Игровые характеристики, рейтинг, зарплата, потенциал и стоимость генерируются собственным движком. Никакие рейтинги FIFA / EA SPORTS FC / Football Manager в базу не копируются.

## Ссылки на использованные клубные статьи

| ID | Статья клуба | ID | Статья клуба |
|---|---|---|---|
| 0 | [Arsenal F.C.](https://en.wikipedia.org/wiki/Arsenal_F.C.) | 18 | [Inter Milan](https://en.wikipedia.org/wiki/Inter_Milan) |
| 1 | [Liverpool F.C.](https://en.wikipedia.org/wiki/Liverpool_F.C.) | 19 | [AC Milan](https://en.wikipedia.org/wiki/AC_Milan) |
| 2 | [Manchester City F.C.](https://en.wikipedia.org/wiki/Manchester_City_F.C.) | 20 | [Juventus FC](https://en.wikipedia.org/wiki/Juventus_FC) |
| 3 | [Chelsea F.C.](https://en.wikipedia.org/wiki/Chelsea_F.C.) | 21 | [SSC Napoli](https://en.wikipedia.org/wiki/SSC_Napoli) |
| 4 | [Manchester United F.C.](https://en.wikipedia.org/wiki/Manchester_United_F.C.) | 22 | [AS Roma](https://en.wikipedia.org/wiki/AS_Roma) |
| 5 | [Tottenham Hotspur F.C.](https://en.wikipedia.org/wiki/Tottenham_Hotspur_F.C.) | 23 | [Atalanta BC](https://en.wikipedia.org/wiki/Atalanta_BC) |
| 6 | [Real Madrid CF](https://en.wikipedia.org/wiki/Real_Madrid_CF) | 24 | [Paris Saint-Germain FC](https://en.wikipedia.org/wiki/Paris_Saint-Germain_FC) |
| 7 | [FC Barcelona](https://en.wikipedia.org/wiki/FC_Barcelona) | 25 | [Olympique de Marseille](https://en.wikipedia.org/wiki/Olympique_de_Marseille) |
| 8 | [Atlético Madrid](https://en.wikipedia.org/wiki/Atl%C3%A9tico_Madrid) | 26 | [AS Monaco FC](https://en.wikipedia.org/wiki/AS_Monaco_FC) |
| 9 | [Athletic Bilbao](https://en.wikipedia.org/wiki/Athletic_Bilbao) | 27 | [Lille OSC](https://en.wikipedia.org/wiki/Lille_OSC) |
| 10 | [Villarreal CF](https://en.wikipedia.org/wiki/Villarreal_CF) | 28 | [Olympique Lyonnais](https://en.wikipedia.org/wiki/Olympique_Lyonnais) |
| 11 | [Real Sociedad](https://en.wikipedia.org/wiki/Real_Sociedad) | 29 | [OGC Nice](https://en.wikipedia.org/wiki/OGC_Nice) |
| 12 | [FC Bayern Munich](https://en.wikipedia.org/wiki/FC_Bayern_Munich) | 30 | [FC Zenit Saint Petersburg](https://en.wikipedia.org/wiki/FC_Zenit_Saint_Petersburg) |
| 13 | [Borussia Dortmund](https://en.wikipedia.org/wiki/Borussia_Dortmund) | 31 | [FC Spartak Moscow](https://en.wikipedia.org/wiki/FC_Spartak_Moscow) |
| 14 | [Bayer 04 Leverkusen](https://en.wikipedia.org/wiki/Bayer_04_Leverkusen) | 32 | [PFC CSKA Moscow](https://en.wikipedia.org/wiki/PFC_CSKA_Moscow) |
| 15 | [RB Leipzig](https://en.wikipedia.org/wiki/RB_Leipzig) | 33 | [FC Krasnodar](https://en.wikipedia.org/wiki/FC_Krasnodar) |
| 16 | [Eintracht Frankfurt](https://en.wikipedia.org/wiki/Eintracht_Frankfurt) | 34 | [FC Lokomotiv Moscow](https://en.wikipedia.org/wiki/FC_Lokomotiv_Moscow) |
| 17 | [VfB Stuttgart](https://en.wikipedia.org/wiki/VfB_Stuttgart) | 35 | [FC Dynamo Moscow](https://en.wikipedia.org/wiki/FC_Dynamo_Moscow) |

**Реестры правок:** для каждой статьи откройте вкладку *View history* (История); для изменения или адаптации исходных таблиц сохраняйте информацию об авторстве и лицензии.

## Важные ограничения

- Источник — не официальный список зарегистрированных игроков от федерации или клуба. Межсезонные трансферы, переходы в аренду и составы молодых команд могут отражаться с задержкой.
- В игре используются **72 клуба**, но 36 команд второго уровня пока вымышленные.
- При отсутствии подтверждённой даты рождения игрок пропускается. Если на клуб не хватило проверенных записей, остальные места заполняются процедурными футболистами, которые помечаются как сгенерированные.
- `real_players.csv` — версия на указанную дату, а не автоматически обновляющаяся интернет-база. Обновляйте вручную или запускайте `tools/build_real_rosters.py`, соблюдая ограничения на запросы.

**Права и товарные знаки:** Football Universe не связан с FIFA, EA SPORTS или Sports Interactive. Логотипы игроков и коммерческие игровые рейтинги не включены.
