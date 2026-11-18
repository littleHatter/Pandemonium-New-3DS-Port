"""Convert the verified game's raw MP4 .dz files into the tested MPEG-1/MP2 format."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('output',type=Path);p.add_argument('--ffmpeg',default='ffmpeg');a=p.parse_args()
# Hashes correspond to the same package verified by prepare_pandemonium.py.
# The manifest beside this tool pins the source files, not transcoder-version-dependent outputs.
expected=json.loads((Path(__file__).with_name('video_source_hashes.json')).read_text())
a.output.mkdir(parents=True,exist_ok=True);report={}
for source,target in [('in_mpg4.dz','intro.mpg'),('out_mpg4.dz','ending.mpg')]:
 f=a.game/source;sha=hashlib.sha256(f.read_bytes()).hexdigest()
 if sha!=expected[source]:raise SystemExit('Unexpected source hash: '+str(f))
 out=a.output/target;temp=out.with_suffix('.mpg.tmp')
 command=[a.ffmpeg,'-v','error','-y','-i',str(f),'-map','0:v:0','-map','0:a:0','-vf','fps=25','-c:v','mpeg1video','-q:v','8','-c:a','mp2','-ar','32000','-ac','1','-b:a','64k','-f','mpeg',str(temp)]
 subprocess.run(command,check=True);temp.replace(out)
 report[target]={'source_sha256':sha,'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'bytes':out.stat().st_size,'command':command}
(a.output/'conversion_manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
