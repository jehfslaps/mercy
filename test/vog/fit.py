import soundfile as sf, numpy as np, glob, os, re
from scipy.optimize import least_squares
from an import r, fs, SEG
def cH(fn):
    X,_=sf.read(fn); x=X[:len(r),0]
    s,e=SEG[1]; a=r[s:e+int(0.5*fs)]; b=x[s:e+int(0.5*fs)]
    N=1<<int(np.ceil(np.log2(len(a)))); A=np.fft.rfft(a,N); B=np.fft.rfft(b,N); f=np.fft.rfftfreq(N,1/fs)
    fc=np.geomspace(12,4000,240); H=[]
    for c in fc:
        m=(f>=c*2**(-1/48))&(f<c*2**(1/48)); H.append(np.sum(B[m]*np.conj(A[m]))/np.sum(np.abs(A[m])**2))
    return fc,np.array(H)
def model(p,f):
    wz,qz,wp,qp=np.exp(p[:4]); s=2j*np.pi*f
    return (s*s+wz/qz*s+wz*wz)/(s*s+wp/qp*s+wp*wp)
if __name__=='__main__':
    D='/mnt/user-data/uploads/Desktop/Voice of God/'
    res={}
    for fn in sorted(glob.glob(D+'vog_*.wav')):
        nm=os.path.basename(fn)[4:-4]
        if 'bypass' in nm or 'flat' in nm: continue
        f,H=cH(fn)
        def err(p):
            m=model(p,f); e=np.log(H/m); return np.concatenate([e.real,0.3*np.angle(H/m)])
        best=None
        for g in [ (2*np.pi*f[np.argmax(abs(H))]) ]:
            for qz in [0.3,1,3]:
              for qp in [0.7,2,5]:
                p0=np.log([g*0.5,qz,g,qp]); r_=least_squares(err,p0)
                if best is None or r_.cost<best.cost: best=r_
        wz,qz,wp,qp=np.exp(best.x)
        rms=np.sqrt(np.mean((20*np.log10(abs(H)/abs(model(best.x,f))))**2))
        print('%-18s fz=%7.1f Qz=%6.3f  fp=%7.1f Qp=%6.3f  rmsdB=%.2f  |Hmax|=%.1f  phase@fp=%.0f'%(nm,wz/2/np.pi,qz,wp/2/np.pi,qp,rms,20*np.log10(abs(H).max()),np.degrees(np.angle(np.interp(wp/2/np.pi,f,H)))))
        res[nm]=best.x
    np.save('fit.npy',res)
