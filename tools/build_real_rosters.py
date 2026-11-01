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


_last_web_request = 0.0
def fetch_json(url, params, timeout=55, tries=5):
    global _last_web_request
    query = urllib.parse.urlencode(params)
    last_error = None
    for attempt in range(tries):
        # Wikimedia requests share an IP quota on cloud runner machines. Keep
        # queries below 20 per minute and honour explicit rate limits.
        now = time.monotonic()
        pause = max(0.0, 3.5 - (now - _last_web_request))
        if pause:
            time.sleep(pause)
        _last_web_request = time.monotonic()
        try:
            request = urllib.request.Request(url + "?" + query, headers={
                "User-Agent": AGENT,
                "Accept": "application/sparql-results+json, application/json",
            })
            with urllib.request.urlopen(request, timeout=timeout) as response:
                return json.load(response)
        except urllib.error.HTTPError as exc:
            last_error = exc
            if exc.code == 429:
                delay = 20 + 10 * attempt
                print(f"Wikimedia HTTP 429: cooling down {delay}s",
                      file=sys.stderr, flush=True)
                time.sleep(delay)
            elif exc.code in (500, 502, 503, 504):
                time.sleep(5 + 6*attempt)
            else:
                raise
        except (urllib.error.URLError, TimeoutError, ValueError) as exc:
            last_error = exc
            time.sleep(5 + 5*attempt)
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


def wiki_wikitext(title):
    data = fetch_json(API, {
        "action": "parse", "page": title, "prop": "wikitext",
        "format": "json", "redirects": "1"
    }, timeout=45)
    return data["parse"]["wikitext"]["*"]


def list_templates(text):
    import re
    # Balanced template parsing: wiki links and nested name templates contain pipes.
    hits = []
    for found in re.finditer(r"\{\{\s*Fs player\b", text, re.IGNORECASE):
        at, i, depth = found.start(), found.start(), 0
        while i < len(text):
            if text[i:i+2] == "{{":
                depth += 1
                i += 2
            elif text[i:i+2] == "}}":
                depth -= 1
                i += 2
                if depth == 0:
                    hits.append(text[at:i])
                    break
            else:
                i += 1
    return hits


def template_fields(body):
    fields = {}
    content = body[2:-2]
    parts = []
    start, i, braces, links = 0, 0, 0, 0
    while i < len(content):
        if content[i:i+2] == "{{":
            braces += 1
            i += 2
        elif content[i:i+2] == "}}":
            braces -= 1
            i += 2
        elif content[i:i+2] == "[[":
            links += 1
            i += 2
        elif content[i:i+2] == "]]":
            links -= 1
            i += 2
        elif content[i] == "|" and braces == 0 and links == 0:
            parts.append(content[start:i])
            start = i + 1
            i += 1
        else:
            i += 1
    parts.append(content[start:])
    for piece in parts[1:]:
        if "=" not in piece:
            continue
        key, value = piece.split("=", 1)
        fields[key.strip().lower()] = value.strip()
    return fields


def squad_section(text):
    import re
    headings = list(re.finditer(
        r"(?m)^(={2,6})\s*([^=\n]+?)\s*\1\s*$", text))
    eligible = ("current squad", "first-team squad", "first team squad",
                "first team", "senior squad", "players", "playing squad")
    for heading in headings:
        title = heading.group(2).strip().lower()
        if title not in eligible:
            continue
        level = len(heading.group(1))
        next_heading = len(text)
        for following in headings:
            if following.start() > heading.start() and len(following.group(1)) <= level:
                next_heading = following.start()
                break
        section = text[heading.end():next_heading]
        if len(list_templates(section)) >= 7:
            return section
    # If no recognized heading, do not blindly include alumni or loan lists.
    return ""


def squad_players(title):
    import re
    text = wiki_wikitext(title)
    section = squad_section(text)
    raw = list_templates(section)
    players = []
    for body in raw:
        fields = template_fields(body)
        markup = fields.get("name", "")
        position = fields.get("pos", "").strip().upper()
        # [[Player article|Display]] or {{sortname|Given|Family}}.
        match = re.search(r"\[\[([^\]\|#]+)(?:\|([^\]]+))?\]\]", markup)
        if match:
            article = match.group(1).strip()
            name = (match.group(2) or article).strip()
        else:
            sorted_name = re.search(
                r"\{\{\s*sortname\s*\|\s*([^|}]+)\|\s*([^|}]+)", markup,
                re.IGNORECASE)
            if not sorted_name:
                continue
            name = sorted_name.group(1).strip() + " " + sorted_name.group(2).strip()
            article = name
        if not article or not name or position not in ("GK", "DF", "MF", "FW"):
            continue
        players.append({
            "title": article, "name": name, "raw_position": position
        })
    found = {}
    for p in players:
        found[p["title"]] = p
    return list(found.values())


def wikidata_for_titles(titles):
    # Fetch QID pageprops in groups of 40 Wikipedia articles per API request.
    qids = {}
    for start in range(0, len(titles), 40):
        batch = titles[start:start+40]
        result = fetch_json(API, {
            "action": "query", "prop": "pageprops", "ppprop": "wikibase_item",
            "redirects": "1", "titles": "|".join(batch), "format": "json",
        })
        norm = {x["from"]: x["to"] for x in result["query"].get("normalized", [])}
        redirect = {x["from"]: x["to"] for x in result["query"].get("redirects", [])}
        page_map = {
            p["title"]: p.get("pageprops", {}).get("wikibase_item")
            for p in result["query"]["pages"].values()
        }
        for title in batch:
            canonical = norm.get(title, title)
            canonical = redirect.get(canonical, canonical)
            if page_map.get(canonical):
                qids[title] = page_map[canonical]
    return qids


def wikidata_birthdates(qids):
    dates = {}
    unique = list(dict.fromkeys(qids))
    for start in range(0, len(unique), 40):
        batch = unique[start:start+40]
        result = fetch_json("https://www.wikidata.org/w/api.php", {
            "action": "wbgetentities", "ids": "|".join(batch),
            "props": "claims", "format": "json",
        })
        for qid, entity in result.get("entities", {}).items():
            claims = entity.get("claims", {}).get("P569", [])
            for claim in claims:
                if claim.get("rank") == "deprecated":
                    continue
                value = claim.get("mainsnak", {}).get("datavalue", {}).get("value", {})
                raw = value.get("time") if isinstance(value, dict) else None
                date = parse_date(raw[1:] if raw and raw.startswith("+") else raw)
                if date:
                    dates[qid] = date
                    break
    return dates


def parse_date(value):
    if not value:
        return None
    try:
        date = dt.date.fromisoformat(value[:10])
        return date if 1978 <= date.year <= 2011 else None
    except (ValueError, TypeError):
        return None


def generate(probe=False):
    # No WDQS/SPARQL dependency; obtain currently published roster facts from Wikipedia
    # and player QID + date of birth from Wikidata (CC0). No scraping ratings.
    all_rosters = {}
    limit = 1 if probe else len(TITLES)
    for cid, title in enumerate(TITLES[:limit]):
        try:
            entries = squad_players(title)
        except Exception as exc:
            print(f"WARNING {cid} {title}: {exc}", file=sys.stderr, flush=True)
            entries = []
        all_rosters[cid] = entries
        print(f"Club #{cid}: {title} — {len(entries)} roster names", flush=True)
    if probe:
        print("EXAMPLE", json.dumps(all_rosters.get(0, [])[:12], ensure_ascii=False))
        return
    titles = list(dict.fromkeys(p["title"] for group in all_rosters.values()
                                for p in group))
    if len(titles) < 100:
        raise RuntimeError(f"Insufficient Wikipedia squad templates: {len(titles)} names")
    qids = wikidata_for_titles(titles)
    births = wikidata_birthdates(list(qids.values()))
    chosen = []
    seen = set()
    positions = {"GK": "GK", "DF": "DC", "MF": "MC", "FW": "ST"}
    for club in range(36):
        count = 0
        for p in all_rosters[club]:
            qid = qids.get(p["title"])
            dob = births.get(qid)
            if not qid or not dob or qid in seen:
                continue
            age = SOURCE_DATE.year - dob.year - (
                (SOURCE_DATE.month, SOURCE_DATE.day) < (dob.month, dob.day))
            if not 16 <= age <= 45:
                continue
            seen.add(qid)
            chosen.append({
                "club_id": club, "wikidata_qid": qid, "name": p["name"],
                "position": positions[p["raw_position"]],
                "birth_date": dob.isoformat(),
            })
            count += 1
            if count >= 28:
                break
    counts = {i: sum(row["club_id"] == i for row in chosen) for i in range(36)}
    if len(chosen) < 100 or sum(n >= 5 for n in counts.values()) < 10:
        raise RuntimeError(f"Insufficient verified rows: {len(chosen)} "
                           f"across {sum(n >= 5 for n in counts.values())} clubs")
    output = ROOT / "data" / "real_players.csv"
    output.parent.mkdir(exist_ok=True, parents=True)
    with output.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file,
            fieldnames=["club_id", "wikidata_qid", "name", "position", "birth_date"],
            delimiter=";")
        writer.writeheader()
        writer.writerows(chosen)
    meta = {
        "snapshot_date": SOURCE_DATE.isoformat(),
        "source": "Wikipedia current squad tables + Wikidata P569 DOB/QID",
        "licenses": "Wikipedia CC BY-SA 4.0, Wikidata CC0 1.0",
        "total_players": len(chosen),
        "clubs_with_5plus": sum(n >= 5 for n in counts.values()),
        "club_counts": {TITLES[i]: counts[i] for i in range(36)},
        "notes": "Public wiki snapshots, may lag transfers. No EA/SI ratings."
    }
    (ROOT / "data" / "real_players_meta.json").write_text(
        json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(meta, ensure_ascii=False, indent=2), flush=True)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--probe", action="store_true")
    args = p.parse_args()
    generate(probe=args.probe)
