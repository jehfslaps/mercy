import numpy as np
from scipy.interpolate import PchipInterpolator as P
U=[0,2.5,5,7.5,10]; A=[0,2.5,5,7.5,10]
G100=[171.27,142.77,114.28,72.90,57.14]
CEN={40:0.3566,100:1.0,200:1.788}
H_A=[1.07,1.047,1.0215,1.0065,1.0]
Q10={40:[4.413,5.066,5.865,7.121,7.338],100:[7.702,8.334,8.837,8.604,7.846],200:[8.08]*5}
QA={40:[0.119,1.182,1.936,4.266,5.865],100:[0.050,0.498,0.992,3.452,8.837]}
KA={40:[0.978,0.810,0.693,0.363,0.143],100:[0.978,0.945,0.896,0.648,0.149]}
K10={40:[0.127,0.133,0.143,0.173,0.199],100:[0.120,0.132,0.149,0.186,0.204]}
lg=lambda v:np.log(v)
def params(center,u,a):
    cc=100 if center==200 else center
    fp=CEN[center]*np.exp(P(U,lg(G100))(u))*np.exp(P(A,lg(H_A))(a))
    q10u=np.exp(P(U,lg(Q10[center]))(u)); q10_5=Q10[center][2]
    qa=np.exp(P(A,lg(QA[cc]))(a))/QA[cc][4]*Q10[cc][2]
    Q=qa*q10u/q10_5 if center!=200 else np.exp(P(A,lg(QA[100]))(a))/QA[100][4]*q10u
    ka=P(A,KA[cc])(a); k10u=P(U,K10[cc])(u); k10_5=K10[cc][2]
    k=1-(1-ka)*(1-k10u)/(1-k10_5)
    return float(fp),float(Q),float(k)
def resp(f,fp,Q,k):
    s=2j*np.pi*np.asarray(f); wp=2*np.pi*fp
    return (s*s+k*wp/Q*s)/(s*s+wp/Q*s+wp*wp)
def biquad(fs,fp,Q,k):
    # bilinear with prewarp at fp: s = K (1-z^-1)/(1+z^-1), K = wp/tan(wp/2fs)
    wp=2*np.pi*fp; K=wp/np.tan(wp/(2*fs))
    a2,a1,a0=1.0,k*wp/Q,0.0     # numerator s^2 + a1 s
    b2,b1,b0=1.0,wp/Q,wp*wp
    def bl(c2,c1,c0):
        return np.array([c2*K*K+c1*K+c0, 2*c0-2*c2*K*K, c2*K*K-c1*K+c0])
    n=bl(a2,a1,a0); d=bl(b2,b1,b0); return n/d[0], d/d[0]
