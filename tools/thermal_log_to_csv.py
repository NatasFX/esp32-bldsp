"""Follow the shared PuTTY log without opening/locking COM7.
host_read_utc is ingestion time; use ESP uptime_ms for sample timing.
"""
import argparse,csv,re,time
from pathlib import Path
from datetime import datetime,timezone

FIELDS=['kind','host_read_utc','boot','uptime_ms','session','phase','valid','ntc_c','mean10_c','n','raw','mv',
        'coil_pred_c','ntc_pred_c','action','source_v','limit_a','ambient_c','ref_v','ref_i','ref_coil_c','ref_body_c']
def parse(line):
    match=re.search(r'\b(THERMAL|MARK) t_ms=(\d+)\b(.*)',line)
    if not match:return None
    row=dict(re.findall(r'(\w+)=([^\s\x1b]+)',match[3]))
    row.update(kind=match[1],uptime_ms=match[2])
    return {k:('' if str(v).lower()=='nan' else v) for k,v in row.items() if k in FIELDS}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('log',type=Path);ap.add_argument('csv',type=Path);ap.add_argument('--once',action='store_true');args=ap.parse_args()
    args.csv.parent.mkdir(parents=True,exist_ok=True)
    with args.log.open('rb') as src,args.csv.open('w',newline='',encoding='utf-8-sig') as target:
        writer=csv.DictWriter(target,fieldnames=FIELDS);writer.writeheader();pending=b'';boot=1;last=0
        while True:
            data=src.read(65536)
            if data:
                pending+=data;lines=pending.split(b'\n');pending=lines.pop()
                for line in lines:
                    row=parse(line.decode('utf-8',errors='replace'))
                    if row:
                        now=int(row['uptime_ms'])
                        if now+5000<last:boot+=1
                        last=now;row['boot']=boot;row['host_read_utc']=datetime.now(timezone.utc).isoformat();writer.writerow(row)
                target.flush()
            elif args.once:break
            else:
                if args.log.stat().st_size<src.tell():src.seek(0);pending=b'';boot+=1;last=0
                time.sleep(.3)
if __name__=='__main__':main()
