#!/usr/bin/env python3
"""Build a reproducible, offline, licensed global football identity index.

Input: withqwerty/reep v0 data/people.csv, frozen snapshot dated 2026-06-21,
CC0-1.0. It contains player AND coach rows; only type='player' is imported.
No active-player status, verified club or football-manager ratings are claimed.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import pathlib
import re
import urllib.request

SOURCE='https://raw.githubusercontent.com/withqwerty/reep/main/data/people.csv'
COLUMNS=['source_id','wikidata_qid','name','position','birth_date','nationality']

def clean(value, limit=140):
    return ' '.join(str(value or '').replace(';',',').replace('\x00','').split())[:limit]

def build(source, dest, metadata, min_year=1981, max_year=2011):
    dest=pathlib.Path(dest);dest.parent.mkdir(parents=True, exist_ok=True)
    sha=hashlib.sha256()
    stats={'rows_read':0,'coaches_skipped':0,'players_missing_name':0,
           'players_unknown_birthday':0,'players_outside_age_range':0,
           'players_without_position':0,'duplicate_ids':0,'duplicate_wikidata':0,
           'players_written':0,'distinct_nationalities':0}
    ids=set();qids=set();countries=set()
    with open(source,encoding='utf-8-sig',newline='') as inp, open(dest,'w',encoding='utf-8',newline='') as out:
        rd=csv.DictReader(inp);wr=csv.writer(out,delimiter=';',lineterminator='\n',quoting=csv.QUOTE_MINIMAL)
        wr.writerow(COLUMNS)
        if not {'reep_id','type','name','date_of_birth','position'}.issubset(rd.fieldnames or []):
            raise ValueError('Unexpected Reep v0 people CSV columns: '+str(rd.fieldnames))
        for record in rd:
            stats['rows_read']+=1
            if record.get('type','').strip().lower()!='player':
                stats['coaches_skipped']+=1;continue
            name=clean(record.get('name'))
            if len(name)<3 or len(name)>140:
                stats['players_missing_name']+=1;continue
            pid=clean(record.get('reep_id'),40)
            if not re.fullmatch(r'reep_p[a-zA-Z0-9_]{2,32}',pid):continue
            dob=clean(record.get('date_of_birth'),12)[:10]
            if dob:
                try:
                    date=dt.date.fromisoformat(dob)
                except ValueError:
                    dob=''
                else:
                    if date.year<min_year or date.year>max_year:
                        stats['players_outside_age_range']+=1;continue
            if not dob:stats['players_unknown_birthday']+=1
            if pid in ids:stats['duplicate_ids']+=1;continue
            qid=clean(record.get('key_wikidata'),22)
            if qid and not re.fullmatch(r'Q[1-9][0-9]*',qid):qid=''
            if qid and qid in qids:
                stats['duplicate_wikidata']+=1;continue
            position=clean(record.get('position') or record.get('position_detail'),70)
            if not position:stats['players_without_position']+=1
            country=clean(record.get('nationality'),70)
            ids.add(pid)
            if qid:qids.add(qid)
            if country:countries.add(country)
            wr.writerow([pid,qid,name,position,dob,country]);stats['players_written']+=1
    stats['distinct_nationalities']=len(countries)
    with dest.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):sha.update(chunk)
    stats.update({'source':SOURCE,'source_snapshot':'2026-06-21 (frozen v0)',
                  'license':'CC0-1.0','age_birth_year_filter':[min_year,max_year],
                  'important':'Identity index, NOT verified active 2026/27 rosters or free agents.',
                  'sha256':sha.hexdigest(),'generated_utc':dt.datetime.now(dt.timezone.utc).isoformat()})
    pathlib.Path(metadata).write_text(json.dumps(stats,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(stats,indent=2))
    if stats['players_written']<20000:
        raise RuntimeError('Unexpectedly few entries: '+str(stats['players_written']))

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',help='Locally supplied CSV (download not required)')
    ap.add_argument('--output',default='data/world_players.csv')
    ap.add_argument('--metadata',default='data/world_players_meta.json')
    args=ap.parse_args()
    if args.input:build(args.input,args.output,args.metadata)
    else:
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/'people.csv'
            request=urllib.request.Request(SOURCE,headers={'User-Agent':'FootballUniverse-database-builder/0.16'})
            with urllib.request.urlopen(request,timeout=180) as reply, path.open('wb') as f:
                for chunk in iter(lambda:reply.read(1024*1024),b''):f.write(chunk)
            build(path,args.output,args.metadata)
