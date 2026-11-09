MERCY  -  drum processor  -  by slapworx
version 1.0  -  Windows 64-bit (VST3 + CLAP)  -  Mac, Apple Silicon (AU + VST3 + CLAP)
slapworx.net

The full guide is MERCY_User_Manual.pdf, in this folder.


WHAT MERCY IS
-------------
A drum processor inspired by three eras of Kanye West: My Beautiful Dark
Twisted Fantasy, Watch the Throne and Yeezus. Put it on a break, a loop, a
kit bus, a kick or a snare.

- THREE ERAS. fantasy, throne and yeezus each voice the whole chain.
- 76 PRESETS in seven groups: start, punch, grit, low end, space, gated, eras.
- TEN MODULES on the advanced page, in any order: transient, shape, sampler
  (sp-12 / asr / mpc), preamp, squash, tone, weight, verb, clip and
  gate/width. Each has its own presets (click its name).
- BAKE. Print MERCY's sound into audio files inside the plugin: drop audio,
  bake, drag the result into your DAW. One file, a batch, or a whole folder.
- CAPTURE. Grab the last 1-16 bars you just heard, or record from the next
  bar line, snapped to your DAW's bars so loops come out whole.
- Same sound on Windows and Mac (every build renders bit-identical audio).


INSTALL - WINDOWS
-----------------
1. Close your DAW.
2. Double-click "Install MERCY.exe". If Windows says "Windows protected your
   PC", click More info > Run anyway.
3. Keep VST3 and CLAP ticked (or untick the one you don't use) > Install.
4. Open FL Studio: Options > Manage plugins > Find installed plugins.
Uninstall: Settings > Apps > Installed apps > MERCY (slapworx).
By hand instead: copy the files in "manual install" to
C:\Program Files\Common Files\VST3\ and ...\CLAP\ (unblock the zip first:
right-click > Properties > Unblock).


INSTALL - MAC (Apple Silicon: M1 or newer)
------------------------------------------
1. Quit your DAW.
2. Double-click "Install MERCY.pkg". If macOS can't check it, right-click it
   > Open > Open.
3. Continue, keep Audio Unit / VST3 / CLAP ticked > Install (your Mac
   password).
4. Open your DAW. Logic / GarageBand: Audio Units > slapworx > Mercy. If it
   doesn't appear: Logic > Settings > Plug-in Manager > select it >
   "Reset & Rescan Selection".
By hand instead: copy the files in "manual install" to /Library/Audio/Plug-Ins/
(Components, VST3, CLAP).
Intel Macs aren't supported.


THE MAIN PAGE
-------------
fantasy / throne / yeezus   The era. Changes the voicing AND loads that
                            era's starting point into the advanced page
                            (transient and shape are left alone).
squash   Compression + parallel blend. 12 o'clock = the advanced settings.
dirt     Preamp saturation into the clipper. Same 12 o'clock rule.
age      Sampler stage: lower rate, fewer bits, pitch crunch. Zero = off.
input / mix / output, auto gain
transient |-> 100   Ableton-style trim. Drag down; red box = active.
fig. 01  Live output waveform (last second) with the peak marked; the small
         square is the input.
in / gr / out   Meters. gr = gain reduction (red part).
hpf / lpf    Soft output filters. Far left / far right = off.
bottom bar   presets (click the name for the list), save, a/b, bypass,
             quality (2x = cleaner).
light / dark   Top right. Switches the look.


THE ADVANCED PAGE
-----------------
Ten modules in signal order (numbered 1-10). Drag a module by its name onto
another slot to reorder; "reset order" puts it back. The red square on each
module turns it on/off. Click a module's name for its own presets.
"eq pre/post" (clip) swaps tone and clip. Verb and weight start off.
"bake" (bottom right) opens the bake panel: single, bulk and capture.


CONTROLS
--------
drag = adjust   shift+drag = fine   double-click = reset   wheel = nudge
right-click = type a value, Enter to confirm (Esc cancels)
bottom-right corner = drag to resize (75% to 200%)
save = name and save a user preset. In the preset list, right-click a
       user preset to rename or delete it (or hover it and click the x).
User presets: Windows  %APPDATA%\Mercy\user_presets.txt
              Mac      ~/Library/Application Support/Mercy/user_presets.txt
The file is the same on both, so you can copy your presets between them.


KNOWN LIMITS
------------
- While a name box or typed value is open, your DAW's keyboard shortcuts
  (space to play, etc.) don't reach it. They come back as soon as you press
  Enter / Esc or close the box.
- DAW automation lanes show raw numbers (e.g. 0.5 instead of 50%).
- Shape adds level when boosting (and auto gain evens it out if on).
  With auto gain off, use output to level-match before judging.


CREDITS
-------
Built with DPF (DISTRHO Plugin Framework, ISC license), the fonts Archivo,
Unbounded and JetBrains Mono (SIL Open Font License) and dr_libs (public domain / MIT-0). Licenses are in the licenses folder.
Not affiliated with or endorsed by Kanye West or Ableton.
