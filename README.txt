MERCY  -  drum processor
version 1.9  -  Windows 64-bit (VST3)  -  Mac, Apple Silicon (VST3 + AU)


WHAT'S NEW IN 1.9
-----------------
- MAC. MERCY now runs on Apple Silicon Macs (M1 and newer) as VST3 and
  AU (for Logic Pro and GarageBand). Same sound, same look, same presets
  as on Windows.
- Same sound on every computer: Windows and Mac now render bit-identical
  audio (every build is checked). Compared with 1.8.1 the difference is
  far below anything you can hear (around -110 dB).
- CLAP is no longer included. Use the VST3 (FL Studio, Ableton, Cubase,
  Studio One, Reaper, Bitwig...) or the AU on Mac.
- Shows up under "slapworx" in your plugin list.


WHAT'S NEW IN 1.8.1
-------------------
- FIX: typing now works. When the name box (or a typed value) is open,
  MERCY takes the keyboard itself, then hands it back to FL when you
  press Enter / Esc or close the box. (1.8 relied on FL passing the keys
  on, which it doesn't.)
- "open presets folder" at the bottom of the preset list opens the folder
  with your user_presets.txt (it's in a hidden Windows folder otherwise).


WHAT'S NEW IN 1.8
-----------------
- NAME YOUR PRESETS. "save" now opens a small box with a name field. Type
  a name and press Enter (or click "save"). The suggested name ("user 03")
  is selected, so you can just start typing. Esc or "cancel" backs out.
- RENAME AND DELETE user presets. In the preset list (click the preset
  name), right-click one of your presets for "rename" or "delete". You can
  also hover it and click the small x. Delete always asks "delete? yes /
  no" first. Factory presets can't be changed.
  If you delete the preset you're using, your sound stays as it is; the
  name just goes back to "default break *" since it's no longer saved.
- Bake: the drop zone now says "drop audio here". Any audio works, not
  just loops: one-shots, stems, full beats.


WHAT'S NEW IN 1.7
-----------------
- A NEW LOOK. MERCY has a whole new interface: Swiss grid layout, print
  grain, flat color blocks, and the sound drawn as data. Same controls in
  the same places, same sound, same presets, your projects open as before.
- DARK AND LIGHT. MERCY opens dark. Click "light / dark" at the top right
  to switch. Your choice is saved with the project.
- LIVE SPECIMEN (fig. 01). The mint panel on the main page draws MERCY's
  real output over the last second, like a printed waveform. The crosshair
  sits on the loudest hit and shows its peak level. The small square in
  the corner is the input, so you can see before and after at a glance.
  (The specimen is scaled to its own loudest moment, so it always fills
  the panel; the peak label is the true level.)
- in / gr / out meters now draw as bars (gr in red).
- The era sets the accent color: fantasy blue + violet, throne mint +
  pink, yeezus red + orange.
- Module figures on the advanced page react to your settings: transient
  markers and fade, the preamp drive curve, the clip ceiling, the stereo
  field, plus tiny header graphs for sampler, squash, tone and shape.
- The preset list, module presets and the bake panel are restyled to
  match.


WHAT'S NEW IN 1.6
-----------------
- MERCY REMEMBERS WHERE YOU WERE. Close the plugin window and open it again
  and you're back on the same page: advanced or main, bake panel open or
  closed, single or bulk, your bake file list (with drag handles), the
  preset name, and your A/B slots. Bakes keep running in the background
  while the window is closed.
- The page, bake panel and preset are also saved with your FL project, so
  reopening a project puts you back where you left off. (The bake file list
  isn't stored in the project; your baked files are always in
  Documents\Mercy Bakes, or Documents/Mercy Bakes on a Mac.)
- Clicking somewhere else in the plugin no longer closes the bake panel
  (for example, the click that brings the window back after dragging a
  file into FL). Close it with the x, the "bake" button, or Esc.
- Technical: the VST3 is now a single combined component (needed so the
  window can share the session with the audio part). Existing projects
  load as before.


WHAT'S NEW IN 1.5
-----------------
- BAKE: print MERCY's sound straight into audio files, inside the plugin.
  Advanced page > "bake" (bottom right). The panel stays hidden until then.
    1. Dial in a sound you like.
    2. Drag audio (wav, aiff, flac, mp3: loops, one-shots, stems...) from
       FL's browser or Explorer onto the drop zone, or click "or browse".
    3. Press "bake".
    4. Grab the file by its "drag ->" handle and drop it into FL (playlist,
       channel rack, a sampler...). Drop the next loop and repeat.
  single  keeps one file at a time: drop, bake, drag, drop the next one.
  bulk    drop many files at once, "bake all", then drag them one by one or
          all together with "drag all ->".
  Change your settings and press bake again to re-bake with the new sound.

  What a bake does:
  - Uses everything you hear: era, knobs, every module, chain order,
    transient, shape, filters, mix, output, auto gain.
  - Always renders at 2x quality, and ignores bypass.
  - Keeps the exact length and timing of the original. Loops wrap
    seamlessly: MERCY warms up on the loop first, so the first hit sounds
    like every other hit and the end flows into the start.
  - Writes 24-bit WAV at the file's own sample rate. If the result goes
    over 0 dB it's saved as 32-bit float instead, so nothing ever clips
    (shown as "32-bit" in the list). Turn "output" down if you prefer 24-bit.
  - Files are named after the original + the preset, e.g.
    amen_mercy_gold-throne.wav, and saved in Documents\Mercy Bakes
    (Mac: Documents/Mercy Bakes; "open bake folder" opens it). Nothing is
    overwritten.
  - Runs in the background, so FL keeps playing smoothly. Closing the
    plugin window stops a bake in progress.
  Notes:
  - MP3s carry a little encoder silence at the edges, so MP3 bakes come
    out slightly longer and won't loop perfectly. Use WAV for loops.
  - With the sampler stage on, each pass of a loop varies very slightly
    (the low-sample-rate grid lands differently every time, live too), so a
    bake can't be bit-identical to a given live pass. With the sampler
    off, a bake is bit-identical to what MERCY plays live.
  - If dragging a file in or out ever doesn't work in your DAW, use
    "or browse" to load and "open bake folder" to grab the files.


WHAT'S NEW IN 1.4
-----------------
- "advanced" now toggles: click it again to go back to the main page.
- MODULE PRESETS (hidden): on the advanced page, click a module's name
  (sampler, squash, tone...) and a small preset box opens for just that
  module. Picking one only changes that module. "default" puts the module
  back to its starting point (for era-driven modules, the current era's
  setting). The preset that matches the module's current settings is
  marked in red. Dragging a name still reorders the chain; a click opens
  the presets.
    transient   light trim, tight, chopped, gated, catch ghosts, big hits only
    shape       kick punch, kick boom, snare crack, fat snare, loop snap,
                dry loop, thick loop, soften harsh
    sampler     sp-12 style, sp-12 pitched down, asr warm, asr grit,
                mpc60 style, mpc 2000 clean, crushed, am radio
    preamp      console glue, console push, tube warmth, tube burn, tape,
                tape hot, iron, iron fuzz
    squash      bus glue, punch, all buttons, parallel smash, pump,
                room lift, gentle
    tone        warm, big low end, 100 hz thump, snare bite, air, dark, radio
    clip        clean, soft kiss, soft loud, hard edge, hard slam,
                low ceiling grit
    gate/width  wide, open stereo, narrow, mono, tight gate, choke
- Nothing about the sound changes: same defaults as 1.3.


WHAT'S NEW IN 1.3
-----------------
- HPF / LPF: soft high-pass and low-pass filters, under the meters on the
  main page. Gentle slope (-6 dB at the cutoff, then 12 dB per octave).
  hpf 20 Hz (off) to 2 kHz, lpf 20 kHz (off) to 500 Hz. They sit after
  auto gain, so auto gain won't undo them.
- PRESET BROWSER: click the preset name in the bottom bar to open the list.
  Click a preset to load it; click outside, the x, or press Esc to close.
  Mouse wheel scrolls when your user presets don't fit.
- 20 NEW PRESETS (34 total): fantasy break, lush room, gold throne,
  hotel mix, stadium snare, black skin, industrial kick, blown 808 room,
  sp-12 crunch, asr soul chop, mpc thump, tape glue, all buttons,
  parallel smash, dry and tight, chopped hats, dusty vinyl,
  telephone break, big room toms, melodic sample.
- Signature in the bottom-right corner.
- Projects from earlier versions load with both filters off.
- Heads-up: on breaks that already peak near 0 dBFS, some presets can peak
  a little above 0 (auto gain restores loudness after trimming). FL Studio
  handles this fine; pull "output" down if your master clips.


WHAT'S NEW IN 1.2
-----------------
- SHAPE: a drum transient shaper on the advanced page (section 2, right
  after transient). Turns the attack of each hit up or down independently
  from its sustain/tail.
    kick / snare / bus        what you're feeding it. Tunes hit detection
                              and decides what "focus" works on.
    classic / natural / smooth   character.
        classic  raw, a bit pumpy, softer attack boost
        natural  clean and transparent (default)
        smooth   slower and fuller, closer to parallel compression
    attack    -100% to +100%   more snap / softer front
    sustain   -100% to +100%   longer, fatter tails / drier, tighter
    focus     shaping aimed at one frequency range. Its name changes
              with the mode:
        kick  -> tight     + = punchier, less boom   - = boomier
        snare -> body      + = more body punch (~240 Hz)
        bus   -> presence  + = more bite up top      - = softer
  Everything at 0% = no change at all (bit-for-bit identical to 1.1).
  It uses MERCY's existing 5 ms look-ahead, so attack boosts catch the very
  front of each hit. No extra latency.
- Two new presets: "snap" and "fat room".
- Transient vs shape: transient (the Ableton one) only removes, by
  restarting a fade on every hit. Shape can also ADD attack or sustain.
- Projects and user presets from 1.0 and 1.1 load fine; shape starts at 0.

Starting points (suggestions):
  tighter kick    kick, natural, attack +40, sustain -20, tight +40
  snappy snare    snare, natural, attack +50, then body to taste
  dry roomy loop  bus, natural, sustain -50
  thick loop      bus, smooth, sustain +40


INSTALL - WINDOWS
-----------------
0. Before unzipping: right-click the zip > Properties > tick "Unblock" > OK.
1. Close FL Studio.
2. Copy the "Mercy.vst3" folder from VST3\ into
       C:\Program Files\Common Files\VST3\   (replace the old one)
3. If you installed the 1.8 CLAP before, you can delete
       C:\Program Files\Common Files\CLAP\Mercy.clap
   (old projects that used the CLAP: swap in the VST3).
4. Open FL Studio: Options > Manage plugins > Find installed plugins.


INSTALL - MAC (Apple Silicon: M1 or newer)
------------------------------------------
1. Quit your DAW.
2. In Finder press Shift+Cmd+G and go to  ~/Library/Audio/Plug-Ins
3. Copy "Mercy.component" into the Components folder (AU: Logic,
   GarageBand) and "Mercy.vst3" into the VST3 folder (other DAWs).
4. Open your DAW. Logic / GarageBand: MERCY is under
   Audio Units > slapworx > Mercy. Logic checks new plugins the first
   time it starts; if MERCY doesn't appear, open Logic > Settings >
   Plug-in Manager, select it and click "Reset & Rescan Selection".
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
Eight stages in signal order (numbered 1-8). Drag a section by its name
onto another to reorder; "reset order" puts it back. The red square on each
section turns it on/off. "eq pre/post" (clip section) swaps tone and clip.
Transient stage: envelope (same as the main box) and detect (how easily
hits are picked up).


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
