import soundfile as sf, numpy as np, sys, glob, os
R,fs=sf.read('MERCY_weight_test_kit.wav'); r=R[:,0]
# segments of the kit
nz=np.abs(r)>1e-7
edges=np.flatnonzero(np.diff(nz.astype(int)))
def regions():
    out=[];s=None
    for i,v in enumerate(nz[::64]):
        pass
regs=[]; i=0; n=len(r)
idx=np.flatnonzero(nz)
# split where gap > 0.3s
gaps=np.flatnonzero(np.diff(idx)>int(0.3*fs))
starts=[idx[0]]+[idx[g+1] for g in gaps]; ends=[idx[g] for g in gaps]+[idx[-1]]
SEG=list(zip(starts,ends))
def tf(x, seg, nfft=None):
    s,e=seg; s0=s; e0=e+int(0.5*fs)
    a=r[s0:e0]; b=x[s0:e0]
    N=1<<int(np.ceil(np.log2(len(a))))
    A=np.fft.rfft(a,N); B=np.fft.rfft(b,N)
    f=np.fft.rfftfreq(N,1/fs)
    return f,A,B
def smooth_db(f,A,B,cents=1/24):
    fc=np.geomspace(12,18000,400); out=[]
    for c in fc:
        lo,hi=c*2**(-cents),c*2**(cents); m=(f>=lo)&(f<hi)
        out.append(10*np.log10(np.sum(np.abs(B[m])**2)/np.sum(np.abs(A[m])**2)))
    return fc,np.array(out)
if __name__=='__main__':
    print('segments (s):',[(round(s/fs,3),round(e/fs,3)) for s,e in SEG])
    for fn in sys.argv[1:]:
        X,_=sf.read(fn); x=X[:len(r),0]
        fc,d18=smooth_db(*tf(x,SEG[1])); fc,d6=smooth_db(*tf(x,SEG[2]))
        np.save('meas/'+os.path.basename(fn)[:-4]+'.npy',np.vstack([fc,d18,d6]))
        k=np.argmax(d18)
        print(os.path.basename(fn),'peak %.1f dB @ %.1f Hz'%(d18[k],fc[k]), ' -6dB sweep peak %.1f'%d6.max(),
              ' @20Hz %.1f @30 %.1f @60 %.1f @100 %.1f @200 %.1f @1k %.1f'%tuple(np.interp([20,30,60,100,200,1000],fc,d18)))
