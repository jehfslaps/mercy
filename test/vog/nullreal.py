import numpy as np, soundfile as sf, glob, os, re, subprocess
from an import r, fs, SEG
R,_=sf.read('MERCY_weight_test_kit.wav',dtype='float32'); R.astype(np.float32).tofile('/tmp/kit.f32')
D='/mnt/user-data/uploads/Desktop/Voice of God/'
rows=[]
for fn in sorted(glob.glob(D+'vog_*.wav')):
    nm=os.path.basename(fn)[4:-4]
    m=re.match(r'\d+_(c40|c100|both|none)_f([\d.]+)_a([\d.]+)(_flat)?$',nm)
    if not m or m.group(4): continue
    c={'c40':0,'none':0,'c100':1,'both':2}[m.group(1)]
    subprocess.run(['../wtrun','/tmp/kit.f32','/tmp/out.f32',str(c),m.group(2),m.group(3),'0'],check=True)
    y=np.fromfile('/tmp/out.f32',dtype=np.float32).reshape(-1,2)[:,0].astype(float)
    X,_=sf.read(fn); x=X[:len(y),0]
    def nul(seg):
        s,e=seg; d=x[s:e]-y[s:e]; return 20*np.log10(np.sqrt(np.mean(d**2))/np.sqrt(np.mean(x[s:e]**2)))
    # level difference of the loops (what you would hear as louder/quieter)
    s,e=SEG[3]; lv=20*np.log10(np.sqrt(np.mean(y[s:e]**2))/np.sqrt(np.mean(x[s:e]**2)))
    rows.append((nm,nul(SEG[1]),nul(SEG[3]),lv))
    print('%-18s sweep null %6.1f dB   loop null %6.1f dB   loop level diff %+.2f dB'%rows[-1])
