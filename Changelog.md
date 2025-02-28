VoodooPS2 Changelog
============================
#### v1.0.6
- added middle button scrolling for V8 tracksticks
- made V8 trackstick slower by cutting the speed in a third
- prevent the trackstick pressure from being saved in the touchpad pressure on V8
- removed ability to update logical dimensions in VoodooInput
    - After a while, V8 touchpads break for some reason. Removing fixes that. (Huge thanks to Jake Lo for testing)
- prevent unsupported V9 loading with V8 properties
- removed xupmm/yupmm scaling
- let all ALPS touchpads report their dimensions
- simplified V8 properties
- simplified fingerCount for V3, V4, V5, V7 and V8
- removed logical_min reporting
- touchpad will now enabled when it is restarted disabled
- touchpad will be disabled until probing is done
- removed VoodooInput support for V1, V2 and V6 touchpads
    - V1, V2 and V6 touchpads only report 1 finger, which is not enough to benefit from VoodooInput
- fixed touchpad not resetting completely after Ctrl+Alt+PrntScr combo
- fixed notification centre on V7 touchpads
- (BETA) added support for pressure report to enable Force Touch on V3, V4, V5 and V8 touchpads
    - set ForceTouchMode to 2 (3 or 4) inside Info.plist of VoodooPS2Trackpad to enable pressure support
- decreased FingerZ to 1
- fixed V3 Rushmore not initializing
- fixed V3 Rushmore not reporting 1 finger to VoodooInput

#### v1.0.5
- synced code from acidanthera
    - updated project settings
    - keyboard and trackpad can be disabled via Cmd + PrtScr key combo
    - trackpad can be resetted by pressing Ctrl + Alt + PrtScr (useful for those who have broken trackpad after sleep)
    - PrntScr remap is disabled by default -> re-enable with SSDT-PrtSc-Remap.dsl
    - added NumLock support
- renamed VoodooPS2Controller and VoodooPS2Keyboard BundleIdentifier to acidanthera
- cleaned up project file
- set minimum version to 10.11 (El Capitan)
- resolution of touchpad gets saved as property
- touchpad version gets saved as property
- added support for V8 touchpads (huge thanks to forte500 for testing)
    - made V8 independent of alps_parse_hw_state
    - removed scaling for V8 touchpads
    - disabled pressure report for V8 touchpads
    - added more logging for V8
- removed resolution multiplier which was used on V8 for debugging reasons
- added 'Clickpad' property
- removed hard-coded capabilities of V8 touchpads
- Fixed an issue where otp values were stored in the wrong otp
- specified cmd manually on V8
- removed workaround for making trackpad larger on V8
- added 'Trackpoint' property for V8 touchpads
- added 'X/Y Max' and 'X/Y Res' property for V8 touchpads
- updated HIDScrollResolutionX
- fixed trackstick on V8 (huge thanks to PMD for testing)
- fixed touchpad not working after sleep (mainly for V8) (huge thanks to forte500 and PMD for testing)
- removed scaling for V7 touchpads
- do not probe on fail
- added kernel level USB and bluetooth mouse notifications
- force Force Touch to be disabled when ForceTouchMode is set to 1 and booted in recovery
- added inital partial support for V6 (not tested)
- made V6 independent of alps_parse_hw_state
- removed V6 reporting multiple fingers to VoodooInput
- disabled pressure support for V6
- disabled pressure support for V3/V5
- made V1/V2 independent of alps_parse_hw_state
- disabled pressure support for V1/V2
- added buttons back to V1/V2 to make buttons work when using trackstick
- made V4 independent of alps_parse_hw_state
- ported alps_report_semi_mt_data to V4
- disabled pressure support for V4
- use alps_fields for fingercount on V4
- removed alps_parse_hw_state
- removed redundant codes in set_protocol
- cleaned up alps header
- replaced 12.0 libkmod with 10.11
- fixed some unused codes

#### v1.0.4
- fixed random finger jumps with 2 fingers on V7
- made V7 independent of alps_parse_hw_state
- separated buttons to its own function
- replaced packetReady code with pre-VoodooInput
- added partial support for V3 and V5 (needs testing)
- removed some legacy codes

#### v1.0.3
- made version bumping easier
- ported codes from Linux
- removed warnings
- enabled Trackstick debugging for V8
- fixed packet size
- removed width support
- more debugging code
- added middle button support for V1 & V2
- added resolution multiplier for V8 trackpads
- cleaned up CI

#### v1.0.2
- ported some linux code for V8 (needs to be rewritten tbh)
- fixed VoodooPS2 not loading
- adjusted trackpad resolution for V7 (is a lot nicer)
- added debugging for V8
- made building much more simpler
- archive kext on build
- added VoodooInput on build (VoodooInput is now included)
- a lot of cleanup
- fixed left button only working when a finger was on the touchpad
- added middle button support

#### v1.0.1
- added VoodooInput support for V7 (Huge thanks to usr-sse2 for the code found in VoodooPS2SynapticsTouchpad; Inspiration of kprinssu)
- dropped all other versions (for now; not tested)
- Clean up finger counting logic
- support all gestures
- added physical button support for VoodooInput enabled touchpads
- added proper finger numbering
- fix notification center gesture by adding margins
- support up to 5 fingers
- small bug fixes
- updated some Linux codes
- added partial support for V1 {needs testing}
- added partial support for V2 {needs testing}
- added partial support for V3 {needs testing}
- added partial support for V4 {needs testing}
- added partial support for V5 {needs testing}
- added partial support for V6 {needs testing}
- added partial support for V8 {needs testing}
- implement trackstick buttons
- removed legacy codes
- rebranded to avoid confusion

#### v1.0.0
- upstreamed from 1Revenger1's repo (thanks to 1Revenger1)
- added smoother horizontal scrolling (thanks to icedman)
