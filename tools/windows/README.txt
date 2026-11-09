RICASSO – QUICK START
=====================

1. Extract this ZIP file: right-click the ZIP → “Extract All…” → “Extract”.
   Please don't start it directly from inside the ZIP.
2. In the extracted folder, double-click “Ricasso.exe”.
3. Choose your own Swordigo file or simply drag it onto the window. Done!
   From then on, start the game with the “Swordigo” shortcut on the desktop or in the Start menu.

No terminal, no Python, no installation needed.


“WINDOWS PROTECTED YOUR PC”?
This SmartScreen message appears for programs without a (paid) code signature.
Click “More info” → “Run anyway” – but only if you got the ZIP from a source you trust.
If Windows 11 “Smart App Control” blocks the program completely, you can change that under
Windows Security → App & browser control.


WHERE DO I GET THE SWORDIGO FILE?
Ricasso contains NO game files and never downloads any. You need your own copy of the
Android version (1.4.13 recommended, 1.4.12 works too), e.g. exported from your own phone.
Step-by-step instructions are in Ricasso.exe under “How do I get the file?”.
Please don't download APK files from third-party websites, and don't share the game files.


CONTROLS
Keyboard:   A / D or arrow keys: move      Space or W: jump      Attack/magic: see F2
Controller: left stick or D-pad: move      A: jump      X: attack      Y: magic      B: item
            Start: game menu      In menus: right stick moves the pointer, RT clicks
F1: menu      F2: change keys (R = rebind, F2 = save)      F3: performance overlay


PROBLEMS?
• The game doesn't start or crashes: update your graphics driver (AMD, NVIDIA, Intel).
  Ricasso.exe shows the cause and the log after a crash.
• Your antivirus program reports swordfare.exe: the client translates ARM code at runtime,
  which some scanners wrongly consider suspicious. Only allow it as an exclusion if you trust the source.
• New version of Ricasso: Ricasso.exe shows a button when a newer release is on GitHub
  (checked at most once a day; set RICASSO_NO_UPDATE_CHECK=1 to turn this off).
  Extract the new ZIP, start Ricasso.exe and choose “Reuse previous installation”. Your saves are kept.
• Uninstall: Ricasso.exe → “Uninstall…” removes the shortcuts, the game files and this folder.
  Your saves are kept.
• Log:   %LOCALAPPDATA%\Ricasso\logs\latest.log
• Saves: %APPDATA%\ricasso\save


FOR ADVANCED USERS
The folder “app” contains the actual client (swordfare.exe) together with Start-Swordigo.cmd and
the Python importer tools\windows\import_apkm.py – both work as before.
Licenses: app\LICENSE.md and app\licenses\   Source notes: app\SOURCE.md
The starter Ricasso.exe is licensed under GPL-3.0-or-later; its source code is at
https://github.com/reallinusweiss/ricasso (tools/windows/launcher/).
Bundled libraries and their sources: app\THIRD_PARTY_SOURCES.md


Ricasso's own parts are vibe-coded (written with AI tools). Experimental, no warranty.
Unofficial fan project. Swordigo © Touch Foo. Not affiliated with Touch Foo.
