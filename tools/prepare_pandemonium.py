"""Extract the supplied Pandemonium Game SIS, verifying all embedded SHA-1 hashes."""
import argparse,hashlib,json,lzma,re,struct
from pathlib import Path,PurePosixPath
from sis_support import *
EXPECTED='f9c62764a984d151c3456d8cfb5256423e83f38736e248e32e584d8b0f1f6fa7'

def extract(package,root):
    data=Path(package).read_bytes();digest=hashlib.sha256(data).hexdigest()
    require(digest==EXPECTED,'Unexpected package SHA256: '+digest)
    m=re.search(br'Content-ID:\s*<Game>\s*\r?\n(?:[^\r\n]+\r?\n)*\r?\n',data,re.I)
    require(m is not None,'Missing Game part');sis=data[m.end():]
    require(struct.unpack_from('<I',sis)[0]==0x10201a7a,'Wrong SIS UID')
    kind,contents=next(fields(sis[16:]));require(kind==12,'Missing SIS contents')
    ctrl=decompress(first(contents,3));units=array(first(first(contents,30),2));require(len(units)==1,'Unsupported multi-unit SIS')
    fds=array(first(units[0][1],2));manifest=[];game={};private=None
    for desc in descriptions(ctrl):
        # The FileDescription ends with fixed scalar fields, not more TLVs.
        seq=fields(desc);tag,name_data=next(seq);require(tag==1,'Missing filename')
        name=name_data.decode('utf-16le');index=struct.unpack_from('<I',desc,len(desc)-4)[0]
        require(index<len(fds),'Bad file index');raw=decompress(first(fds[index][1],3))
        # Hash field contains a uint32 algorithm then a SISBlob with the digest.
        hash_payload=None
        for tag,value in seq:
            if tag==25:hash_payload=value;break
        require(hash_payload is not None,'Missing installed-file hash')
        require(struct.unpack_from('<I',hash_payload)[0]==1,'Expected SHA1')
        hash_kind,embedded=next(fields(hash_payload[4:]));require(hash_kind==37, "Unexpected hash blob type "+str(hash_kind))
        require(hashlib.sha1(raw).digest()==embedded,'SHA1 mismatch: '+name)
        norm=name.replace('\\','/').lower();safe=norm.replace(':','').lstrip('/')
        require('..' not in PurePosixPath(safe).parts,'Unsafe path')
        dst=root/'installed'/safe;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(raw)
        entry={'installed_path':name,'index':index,'size':len(raw),'sha1_verified':True,'sha256':hashlib.sha256(raw).hexdigest()}
        # Determine the game's private path from pandemonium.s3e rather than assume a UID.
        manifest.append(entry)
        if norm.endswith('/pandemonium.s3e'):private=norm.rsplit('/',1)[0].split(':',1)[1]+'/'
    require(private is not None,'Missing pandemonium.s3e private directory')
    for e in manifest:
        norm=e['installed_path'].replace('\\','/').lower()
        if norm.split(':',1)[-1].startswith(private):
            rel=norm.split(':',1)[-1][len(private):];raw=(root/'installed'/norm.replace(':','').lstrip('/')).read_bytes();game[rel]=raw
    out=root/'game';out.mkdir(parents=True,exist_ok=True)
    for name,raw in game.items():
        p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
    xe=lzma.decompress(game['pandemonium.s3e'],format=lzma.FORMAT_ALONE)
    require(xe[:4]==b'XE3U','Bad executable');(out/'pandemonium.xe3u').write_bytes(xe)
    report={'package_sha256':digest,'installed_files':len(manifest),'sha1_verified':len(manifest),'private_prefix':private,'xe3u_bytes':len(xe),'xe3u_sha256':hashlib.sha256(xe).hexdigest(),'files':manifest}
    (root/'package_inventory.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='files'},indent=2))
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('package');a.add_argument('output',type=Path);v=a.parse_args();extract(v.package,v.output)
