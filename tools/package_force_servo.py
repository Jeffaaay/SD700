"""Package the committed source and actual ForceServo1 evidence; never hardware I/O."""
import hashlib, json, subprocess, zipfile
from pathlib import Path
from verify_force_servo_firmware import verify, sha

ROOT=Path(__file__).resolve().parent.parent

def git(*args): return subprocess.check_output(['git',*args],cwd=ROOT).decode().strip()

def main():
    assert not git('status','--porcelain'),'Commit reviewable work before packaging'
    assert git('branch','--show-current')=='main'
    result=verify()
    proof=json.loads((ROOT/'Docs/ForceServo1/verification.json').read_text())
    paths=set(subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode().split('\0'))-{''}
    for record in proof['baseline']+proof['final']:
        p=ROOT/record['log']; assert sha(p)==record['sha256'],str(p)
        paths.add(record['log'])
    for name,digest in proof['sources_at_verification'].items():
        assert sha(ROOT/name)==digest,'Source changed after verification: '+name
    for name in ('O0','O2','Os'):
        paths.add('output/ForceServo1/host/'+name+'.log')
    paths.update('output/ForceServo1/'+name for name in (
        'before.json','evidence_preservation.json','host/results.json','host_execution.log',
        'config_verification.log','legacy_config_verification.log',
        'baseline/test_execution.json','final/test_execution.json',
        'regression/test_execution.json','regression/v5_completion_races.log'))
    # Re-verify preservation, including old ignored evidence, without duplicating old ZIPs.
    before=json.loads((ROOT/'output/ForceServo1/before.json').read_text())
    for name,entry in before['preserved_evidence'].items():
        p=ROOT/name
        assert sha(p)==entry['sha256'] and p.stat().st_mtime_ns==entry['mtime_ns'],name
    commit=git('rev-parse','HEAD')
    remote=git('ls-remote','origin','refs/heads/main').split()[0]
    assert remote==commit,'Remote main must match the packaged source commit'
    metadata=dict(candidate='ForceServo1',source_commit=commit,remote_main=remote,
                  baseline=proof['baseline_commit'],physical_test='NOT_RUN',
                  hardware_stop_validation='NOT_VALIDATED',physical_output='LOCKED',firmware=result)
    generated={'SOURCE_OF_TRUTH.json':(json.dumps(metadata,indent=2)+'\n').encode()}
    entries={name:(ROOT/name).read_bytes() for name in sorted(paths)}
    entries.update(generated)
    # Avoid recursively hashing the manifest itself.
    manifest=''.join(hashlib.sha256(data).hexdigest().upper()+' *'+name+'\n'
                     for name,data in sorted(entries.items()))
    entries['ForceServo1_PACKAGE_SHA256SUMS.txt']=manifest.encode()
    target=ROOT/'output/SD700_ForceServo1_SourceOfTruth.zip'
    assert not target.exists(),'Never overwrite a delivered source-of-truth ZIP'
    with zipfile.ZipFile(target,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,data in sorted(entries.items()): archive.writestr('SD700/'+name,data)
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist())=={'SD700/'+n for n in entries}
        for line in manifest.splitlines():
            digest,name=line.split(' *',1)
            assert hashlib.sha256(archive.read('SD700/'+name)).hexdigest().upper()==digest
        assert archive.read('SD700/README.md')==(ROOT/'README.md').read_bytes()
    digest=sha(target)
    target.with_suffix('.zip.sha256').write_bytes((digest+' *'+target.name+'\n').encode())
    print(json.dumps(dict(zip=str(target),zip_sha256=digest,source_commit=commit,
                          verified_entries=len(entries),physical_test='NOT_RUN'),indent=2))

if __name__=='__main__': main()
