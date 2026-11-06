import numpy as np, glob, os
from scipy.optimize import least_squares
from fit import cH
D='/mnt/user-data/uploads/Desktop/Voice of God/'
def model(p,f):
    wp=np.exp(p[0]); Q=np.exp(p[1]); k=p[2]; s=2j*np.pi*f
    return (s*s+k*wp/Q*s)/(s*s+wp/Q*s+wp*wp)
out={}
for fn in sorted(glob.glob(D+'vog_*.wav')):
    nm=os.path.basename(fn)[4:-4]
    if 'bypass' in nm or 'flat' in nm or 'a0' in nm: continue
    f,H=cH(fn); m=f>15
    def err(p):
        e=np.log(H[m]/model(p,f[m])); return np.concatenate([e.real,0.3*e.imag])
    best=None
    for q0 in [0.5,1,3,8]:
        for k0 in [0.1,0.5,0.9]:
            r=least_squares(err,[np.log(2*np.pi*f[np.argmax(abs(H))]),np.log(q0),k0])
            if best is None or r.cost<best.cost: best=r
    wp,Q,k=np.exp(best.x[0]),np.exp(best.x[1]),best.x[2]
    rms=np.sqrt(np.mean((20*np.log10(abs(H[m])/abs(model(best.x,f[m]))))**2))
    mx=np.max(np.abs(20*np.log10(abs(H[m])/abs(model(best.x,f[m])))))
    print('%-18s fp=%7.2f Q=%6.3f k=%6.3f rms=%.3f max=%.2f'%(nm,wp/2/np.pi,Q,k,rms,mx))
    out[nm]=(wp/2/np.pi,Q,k)
np.save('fit2.npy',out)
