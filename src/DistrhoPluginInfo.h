#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "slapworx"
#ifdef MERCY_SAUCE
// "Sauce": the teaser build (CLAP, Windows). Its own name and IDs, so it installs next to MERCY.
#define DISTRHO_PLUGIN_NAME    "Sauce"
#define DISTRHO_PLUGIN_URI     "https://slapworx.com/plugins/sauce"
#define DISTRHO_PLUGIN_CLAP_ID "com.slapworx.sauce"
#define DISTRHO_PLUGIN_BRAND_ID  Slpx
#define DISTRHO_PLUGIN_UNIQUE_ID Sauc
#else
#define DISTRHO_PLUGIN_NAME    "Mercy"
#define DISTRHO_PLUGIN_URI     "https://mercy.audio/plugins/mercy"
#define DISTRHO_PLUGIN_CLAP_ID "audio.mercy.drumprocessor" // MERCY's CLAP is only built on Linux, for the tests
#define DISTRHO_PLUGIN_BRAND_ID  Mrcy
#define DISTRHO_PLUGIN_UNIQUE_ID MrDp
#endif

#define DISTRHO_PLUGIN_HAS_UI       1
#define DISTRHO_PLUGIN_IS_RT_SAFE   1
#define DISTRHO_PLUGIN_NUM_INPUTS   2
#define DISTRHO_PLUGIN_NUM_OUTPUTS  2
#define DISTRHO_PLUGIN_WANT_STATE   1
#define DISTRHO_PLUGIN_WANT_LATENCY 1
#define DISTRHO_PLUGIN_WANT_FULL_STATE 0
#define DISTRHO_UI_USE_NANOVG       1
#define DISTRHO_UI_USER_RESIZABLE   1
#define DISTRHO_UI_FILE_BROWSER     1
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_UI_DEFAULT_WIDTH    960
#define DISTRHO_UI_DEFAULT_HEIGHT   600

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics|Distortion|Stereo"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "compressor", "distortion", "stereo"

#endif
