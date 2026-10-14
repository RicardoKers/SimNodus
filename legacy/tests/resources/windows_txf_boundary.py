# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Disposable TxF isolation observations, not a supported project-save API."""
import argparse
from contextlib import ExitStack, contextmanager
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import mmap
from pathlib import Path
import subprocess
import sys
import local_resources as physical

OLD = b'original owned fixture'
NEW = b'new owned bytes'
INVALID = c.c_void_p(-1).value
CHILD = """import json,sys
from pathlib import Path
p=Path(sys.argv[1])
try:
 if sys.argv[2]=='write':p.write_bytes(b'competing writer')
 else:p.rename(p.with_name('moved.txt'))
except OSError as e:print(json.dumps({'denied':True,'winerror':e.winerror}))
else:print(json.dumps({'denied':False}))
"""


def require(ok):
    if not ok:raise c.WinError(c.get_last_error())


class Api:
    def __init__(self):
        self.fs = physical.WindowsFiles();self.k = self.fs.k
        self.t = c.WinDLL('KtmW32', use_last_error=True)
        self.t.CreateTransaction.argtypes = [c.c_void_p,c.c_void_p,w.DWORD,w.DWORD,w.DWORD,w.DWORD,w.LPWSTR]
        self.t.CreateTransaction.restype = w.HANDLE
        for name in ('CommitTransaction','RollbackTransaction'):
            f = getattr(self.t,name);f.argtypes=[w.HANDLE];f.restype=w.BOOL
        self.k.CreateFileTransactedW.argtypes=[w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE,w.HANDLE,c.c_void_p,c.c_void_p]
        self.k.CreateFileTransactedW.restype=w.HANDLE
        self.k.WriteFile.argtypes=[w.HANDLE,c.c_void_p,w.DWORD,c.POINTER(w.DWORD),c.c_void_p];self.k.WriteFile.restype=w.BOOL
        self.k.SetFilePointerEx.argtypes=[w.HANDLE,c.c_int64,c.c_void_p,w.DWORD];self.k.SetFilePointerEx.restype=w.BOOL
        self.k.SetEndOfFile.argtypes=[w.HANDLE];self.k.SetEndOfFile.restype=w.BOOL
        self.k.GetFinalPathNameByHandleW.argtypes=[w.HANDLE,w.LPWSTR,w.DWORD,w.DWORD];self.k.GetFinalPathNameByHandleW.restype=w.DWORD

    @contextmanager
    def directory(self, path):
        # Reuse retained handle-relative NTFS/reparse/alias checks for every ancestor.
        drive, parts = physical.root_parts(str(path))
        with ExitStack() as stack:
            handle = self.fs.volume(drive, stack)
            for part in parts:handle,_ = self.fs.child(handle,part,True,stack,'$fixture')
            buffer=c.create_unicode_buffer(32768)
            count=self.k.GetFinalPathNameByHandleW(handle,buffer,len(buffer),1)
            assert 0 < count < len(buffer)
            yield handle,Path(buffer.value)

    def capture(self, parent):
        with ExitStack() as stack:
            h,info=self.fs.child(parent,'owned.txt',False,stack,'owned.txt')
            assert info.size() <= 64
            return info.identity(),self.fs.read(h,65,'owned.txt')

    def writer(self, path, tx):
        h=self.k.CreateFileTransactedW(str(path),0xc0000000,7,None,3,0x00200000,None,tx,None,None)
        if h==INVALID:raise c.WinError(c.get_last_error())
        return h

    def write(self,h):
        require(self.k.SetFilePointerEx(h,0,None,0))
        n=w.DWORD();require(self.k.WriteFile(h,NEW,len(NEW),c.byref(n),None));assert n.value==len(NEW)
        require(self.k.SetEndOfFile(h))


def competitor(path, action):
    result=subprocess.run([sys.executable,'-c',CHILD,str(path),action],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stderr
    return json.loads(result.stdout)


def observe(api, root, mode):
    root.mkdir();(root/'owned.txt').write_bytes(OLD)
    report={'mode':mode}
    with api.directory(root) as (parent,pinned):
        path=pinned/'owned.txt';expected_id,expected_bytes=api.capture(parent)
        if mode=='stale-identity':
            path.rename(pinned/'previous.txt');path.write_bytes(OLD)
        elif mode=='stale-bytes':path.write_bytes(b'changed owned fixture')
        original=path.read_bytes()
        with ExitStack() as retained:
            if mode=='existing-writer':
                h=api.k.CreateFileW(str(path),0xc0000000,7,None,3,0x00200000,None)
                require(h!=INVALID);retained.callback(api.k.CloseHandle,h)
            if mode=='writable-map':
                with path.open('r+b') as file:view=mmap.mmap(file.fileno(),0,access=mmap.ACCESS_WRITE)
                retained.callback(view.close)
            tx=api.t.CreateTransaction(None,None,0,0,0,10000,'SimNodus disposable isolation probe')
            require(tx!=INVALID);finished=False
            try:
                try:h=api.writer(path,tx)
                except OSError as error:
                    report['writer_open_error']=error.winerror
                    assert mode in ('existing-writer','writable-map')
                else:
                    try:
                        info=api.fs.info(h,False,'owned.txt');assert info.size()<=64
                        actual=api.fs.read(h,65,'owned.txt')
                        if info.identity()!=expected_id:report['rejected']='identity'
                        elif actual!=expected_bytes:report['rejected']='bytes'
                        else:
                            api.write(h);report['outside_during_write']=path.read_bytes().decode()
                            report['writer_conflict']=competitor(path,'write')
                            report['rename_conflict']=competitor(path,'rename')
                            if mode=='writable-map':
                                view[:3]=b'MAP';view.flush()
                                report['mapped_write_visible']=path.read_bytes().decode()
                    finally:api.k.CloseHandle(h)
                    if mode in ('commit','rollback'):
                        report['outside_after_handle_close']=path.read_bytes().decode()
                        report['late_writer_conflict']=competitor(path,'write')
                        report['late_rename_conflict']=competitor(path,'rename')
                        assert all(report[k]['denied'] for k in ('writer_conflict','rename_conflict','late_writer_conflict','late_rename_conflict'))
                        assert report['outside_during_write']==report['outside_after_handle_close']==OLD.decode()
                    if mode=='commit':require(api.t.CommitTransaction(tx));finished=True
                if not finished:require(api.t.RollbackTransaction(tx));finished=True
            finally:
                if not finished:api.t.RollbackTransaction(tx)
                api.k.CloseHandle(tx)
        report['final_bytes']=path.read_bytes().decode()
        if mode!='writable-map':assert path.read_bytes()==(NEW if mode=='commit' else original)
        if mode.startswith('stale-'):assert report.get('rejected')==mode.removeprefix('stale-')
        if mode=='existing-writer':assert 'writer_open_error' in report
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=args.output.absolute();root.mkdir(parents=True,exist_ok=False)
    report={'status':'started','scope':'Fixture-only observations; no supported TxF save API, crash or platform acceptance','cases':[]}
    try:
        assert sys.platform=='win32';api=Api()
        for mode in ('commit','rollback','stale-identity','stale-bytes','existing-writer','writable-map'):
            report['cases'].append(observe(api,root/mode,mode))
        report['status']='observed'
    except Exception as error:report['status']='failed';report['error']=str(error);raise
    finally:
        report['script_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        (root/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
        print(json.dumps(report))


if __name__=='__main__':main()
