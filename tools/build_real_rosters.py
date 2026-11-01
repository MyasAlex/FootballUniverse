#!/usr/bin/env python3
"""Build a redistributable offline football-roster snapshot from Wikidata CC0.

The first 36 Football Universe clubs are mapped to Wikipedia/Wikidata entities.
Club membership is taken from the latest dated Wikidata P54 statements, NOT
commercial video-game databases. We intentionally do not scrape player ratings.
Usage: python tools/build_real_rosters.py [--probe]
"""
import argparse
import csv
import datetime as dt
import json
import pathlib
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[1]
TITLES = [
    "Arsenal F.C.", "Liverpool F.C.", "Manchester City F.C.", "Chelsea F.C.",
    "Manchester United F.C.", "Tottenham Hotspur F.C.",
    "Real Madrid CF", "FC Barcelona", "Atlético Madrid", "Athletic Bilbao",
    "Villarreal CF", "Real Sociedad",
    "FC Bayern Munich", "Borussia Dortmund", "Bayer 04 Leverkusen", "RB Leipzig",
    "Eintracht Frankfurt", "VfB Stuttgart",
    "Inter Milan", "AC Milan", "Juventus FC", "SSC Napoli",
    "AS Roma", "Atalanta BC",
    "Paris Saint-Germain FC", "Olympique de Marseille", "AS Monaco FC",
    "Lille OSC", "Olympique Lyonnais", "OGC Nice",
    "FC Zenit Saint Petersburg", "FC Spartak Moscow", "PFC CSKA Moscow",
    "FC Krasnodar", "FC Lokomotiv Moscow", "FC Dynamo Moscow",
]
SOURCE_DATE = dt.date(2026, 11, 1)
AGENT = "FootballUniverse/0.12 (offline, educational; github.com/MyasAlex/FootballUniverse)"
API = "https://en.wikipedia.org/w/api.php"
SPARQL = "https://query.wikidata.org/sparql"


def fetch_json(url, params, timeout=55, tries=3):
    query = urllib.parse.urlencode(params)
    last_error = None
    for attempt in range(tries):
        try:
            request = urllib.request.Request(url + "?" + query, headers={
                "User-Agent": AGENT,
                "Accept": "application/sparql-results+json, application/json",
            })
            with urllib.request.urlopen(request, timeout=timeout) as response:
                return json.load(response)
        except (urllib.error.URLError, TimeoutError, ValueError) as exc:
            last_error = exc
            print("HTTP retry", attempt + 1, type(exc).__name__, str(exc)[:150],
                  file=sys.stderr, flush=True)
            time.sleep(2 + attempt * 3)
    raise RuntimeError(f"Data source failed: {url}: {last_error}")


def club_qids():
    # Wikipedia pageprops exposes Wikidata identities, 36 titles in one request.
    result = fetch_json(API, {
        "action": "query", "prop": "pageprops", "ppprop": "wikibase_item",
        "redirects": "1", "titles": "|".join(TITLES), "format": "json",
    })
    pages = result["query"]["pages"]
    pages_by_title = {
        page["title"].casefold(): page.get("pageprops", {}).get("wikibase_item")
        for page in pages.values()
    }
    normalized = {x["from"].casefold(): x["to"].casefold()
                  for x in result["query"].get("normalized", [])}
    redirects = {x["from"].casefold(): x["to"].casefold()
                 for x in result["query"].get("redirects", [])}
    output = []
    for title in TITLES:
        key = title.casefold()
        key = normalized.get(key, key)
        key = redirects.get(key, key)
        qid = pages_by_title.get(key)
        if not qid:
            raise RuntimeError(f"Can't establish Wikidata ID for {title}: {key}")
        output.append(qid)
    return output


def position_code(label):
    text = label.casefold()
    if "goalkeeper" in text:
        return "GK"
    if "left-back" in text or "left back" in text:
        return "DL"
    if "right-back" in text or "right back" in text:
        return "DR"
    if "back" in text or "defender" in text or "sweeper" in text:
        return "DC"
    if "winger" in text or "wide midfielder" in text:
        return "AMR"
    if "forward" in text or "striker" in text or "attacker" in text:
        return "ST"
    if "attacking midfielder" in text:
        return "AMC"
    if "defensive midfielder" in text:
        return "DMC"
    return "MC"


def query_players(qids):
    values = " ".join("wd:" + qid for qid in qids)
    # Include open-ended club memberships and those active on the snapshot date.
    query = f"""
SELECT ?person ?personLabel ?club ?birth ?positionLabel ?start ?end WHERE {{
  VALUES ?club {{ {values} }}
  ?person p:P54 ?membership .
  ?membership ps:P54 ?club .
  OPTIONAL {{ ?membership pq:P580 ?start . }}
  OPTIONAL {{ ?membership pq:P582 ?end . }}
  FILTER(!BOUND(?start) || ?start <= "2026-11-01T23:59:59Z"^^xsd:dateTime)
  FILTER(!BOUND(?end) || ?end >= "2026-08-01T00:00:00Z"^^xsd:dateTime)
  ?person wdt:P569 ?birth .
  OPTIONAL {{ ?person wdt:P413 ?position . }}
  SERVICE wikibase:label {{ bd:serviceParam wikibase:language "en". }}
}}
LIMIT 3000
"""
    data = fetch_json(SPARQL, {"query": query, "format": "json"},
                      timeout=80)
    return data.get("results", {}).get("bindings", [])


def parse_date(value):
    if not value:
        return None
    try:
        date = dt.date.fromisoformat(value[:10])
        if not (1978 <= date.year <= 2011):
            return None
        return date
    except (ValueError, TypeError):
        return None


def generate(probe=False):
    ids = club_qids()
    print("Club Wikidata IDs:", list(zip(TITLES[:6], ids[:6])), flush=True)
    club_by_qid = {q: i for i, q in enumerate(ids)}
    records = {}
    # Six clubs per batch to reduce server impact while limiting query complexity.
    batches = [(0, 1)] if probe else [(i, min(i + 3, 36)) for i in range(0, 36, 3)]
    for start, stop in batches:
        rows = query_players(ids[start:stop])
        print(f"Wikidata clubs {start}-{stop-1}: {len(rows)} statements",
              flush=True)
        for row in rows:
            def val(prop):
                return row.get(prop, {}).get("value", "")
            club = val("club").split("/")[-1]
            person = val("person").split("/")[-1]
            cid = club_by_qid.get(club)
            birth = parse_date(val("birth"))
            name = val("personLabel").strip()
            if cid is None or birth is None or not person.startswith("Q") or not name or name == person:
                continue
            age = SOURCE_DATE.year - birth.year - (
                (SOURCE_DATE.month, SOURCE_DATE.day) < (birth.month, birth.day))
            if age < 15 or age > 45:
                continue
            pos = position_code(val("positionLabel"))
            start_date = parse_date(val("start")) or dt.date(1900, 1, 1)
            key = (person, cid)
            current = records.get(key)
            if current is None or start_date > current[0] or (
                start_date == current[0] and pos != "MC"):
                records[key] = (start_date, {
                    "club_id": cid, "wikidata_qid": person, "name": name,
                    "position": pos, "birth_date": birth.isoformat()
                })
        time.sleep(.6)
    # A player appearing as active for multiple clubs may reflect stale statements:
    # keep only the latest dated membership and discard equal-date ambiguity.
    by_person = defaultdict(list)
    for (qid, cid), item in records.items():
        by_person[qid].append(item)
    valid = []
    for qid, choices in by_person.items():
        choices.sort(key=lambda x: x[0], reverse=True)
        if len(choices) > 1 and choices[0][0] == choices[1][0]:
            continue
        valid.append(choices[0][1])
    groups = defaultdict(list)
    for row in valid:
        groups[row["club_id"]].append(row)
    selected = []
    for club in range(36):
        group = groups[club]
        # Enough goalkeepers, defenders and attackers to form proper XI.
        ranks = {"GK": 0, "DC": 1, "DR": 1, "DL": 1,
                 "DMC": 2, "MC": 2, "AMC": 2, "AMR": 3, "AML": 3, "ST": 3}
        group.sort(key=lambda x: (ranks.get(x["position"], 2), x["birth_date"],
                                  x["name"]))
        selected.extend(group[:30])
    if probe:
        print("PROBE: ", json.dumps(selected[:6], ensure_ascii=False), flush=True)
        return
    if len(selected) < 100 or sum(bool(groups[n]) for n in range(36)) < 12:
        raise RuntimeError(f"Insufficient CC0 coverage: {len(selected)} players, "
                           f"{sum(bool(groups[n]) for n in range(36))} clubs")
    folder = ROOT / "data"
    folder.mkdir(parents=True, exist_ok=True)
    output = folder / "real_players.csv"
    with output.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file,
            fieldnames=["club_id", "wikidata_qid", "name", "position", "birth_date"],
            delimiter=";")
        writer.writeheader()
        writer.writerows(selected)
    summary = {
        "source": "Wikidata P54 club membership, P569 birth, P413 position",
        "license": "CC0 1.0",
        "snapshot_date": SOURCE_DATE.isoformat(),
        "real_player_count": len(selected),
        "clubs_with_players": sum(bool(groups[n]) for n in range(36)),
        "club_counts": {TITLES[i]: len(groups[i][:30]) for i in range(36)},
        "note": "Club memberships may be stale; no commercial ratings copied.",
    }
    (folder / "real_players_meta.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--probe", action="store_true")
    args = p.parse_args()
    generate(probe=args.probe)
