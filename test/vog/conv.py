import numpy as np
from scipy.optimize import minimize
from vogmodel import params, resp
f=np.geomspace(10,2000,3000)
def newpeak(c,u,a,flat):
    fp,Q,k=params(c,u,a)
    if flat: return fp, 20*np.log10(np.sqrt(Q*Q+k*k))
    H=abs(resp(f,fp,Q,k)); i=np.argmax(H); return f[i],20*np.log10(H[i])
def conv(c,kk,amt,flat):
    fo=(50 if c else 20)*4**(kk/10); go=amt*1.2
    best=None
    for cc in (40,100,200):
        def cost(x):
            u,a=np.clip(x,0,10); pf,pg=newpeak(cc,u,a,flat)
            return (12*np.log2(pf/fo))**2+ (pg-go)**2
        for u0 in (1,5,9):
          for a0 in (3,6,9):
            r=minimize(cost,[u0,a0],method='Nelder-Mead',options={'xatol':1e-3,'fatol':1e-4})
            u,a=np.clip(r.x,0,10); cst=cost([u,a])+ (0 if cc==(100 if c else 40) else 0.5)
            if best is None or cst<best[0]: best=(cst,cc,u,a)
    cst,cc,u,a=best; pf,pg=newpeak(cc,u,a,flat)
    return cc,round(u,1),round(a,1),fo,go,pf,pg,cst
for name,c,kk,amt,fl in [('dark plate',0,4,3,0),('big fantasy',0,4,5,0),('thick break',0,5,3,0),('sub bloom',0,2,5,1),('throne kit room',1,2,3,0),('gold plate',1,3,3,0),('kick body',1,2,4,0),('industrial gate',0,3,6,0),('808 weight',0,3,7,0),('crushed but heavy',0,4,4,0),
 ('room into weight',1,3,5,0),('saturated weight',0,4,6,0),('weight last',0,3,6,1),
 ('ERA fantasy',0,4,3,0),('ERA throne',0,5,3,0),('ERA yeezus',0,3,6,0),
 ('M kick weight',0,6,4,0),('M 808 chest',0,4,6,0),('M sub bloom',0,2,5,1),('M kick body',1,2,4,0),('M tight thump',1,4,3,0),('M snare body',1,9,3,1),('M big weight',0,5,8,0)]:
    cc,u,a,fo,go,pf,pg,cst=conv(c,kk,amt,fl)
    print('%-18s old c%d f%g a%g fl%d (%.0fHz %+.1fdB) -> c%d u%.1f a%.1f  (%.0fHz %+.1fdB) cost %.2f'%(name,c,kk,amt,fl,fo,go,cc,u,a,pf,pg,cst))
