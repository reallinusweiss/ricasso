//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

package main

// helpText is shown on the help page "How do I get the file?".
const helpText = `HOW TO GET YOUR SWORDIGO FILE

Swordigo belongs to Touch Foo. Ricasso contains no game files. You need your own copy of the Android version – ideally the one you installed yourself from Google Play on your phone. Don't download APK files from third-party websites, and don't share the game files with others.

Supported: Swordigo 1.4.13 (recommended) and 1.4.12, each in the 64-bit variant (arm64-v8a). Practically every phone from about 2017 onwards has it.

1) ON YOUR PHONE
• Install Swordigo from Google Play (if you haven't already).
• Install an app that can export installed apps including their "split APKs", e.g. "SAI (Split APKs Installer)" (open source: github.com/Aefyr/SAI).
• Select Swordigo in it and export or back it up. You get an .apks file or several .apk files (e.g. base.apk and split_config.arm64_v8a.apk).

2) COPY IT TO YOUR PC
• With a USB cable: connect the phone, choose "File transfer" on the phone and copy the file in File Explorer, e.g. to "Downloads".
• Or via Quick Share, Google Drive, OneDrive or an e-mail to yourself.

3) SET IT UP HERE
• Click "Choose file…" or drag the file onto this window.
• Several .apk files: select all of them in the file dialog (Ctrl+A) or drag them onto the window together. You can also drag a whole folder here.

WHAT HAPPENS?
• The package name, version and checksum of the game library are checked.
• Only what the game needs (game library, "assets" folder, music) is copied – into the subfolder "app".
• Nothing is uploaded or downloaded. Your saves are kept separately in %APPDATA%\swordigo-desktop.

NEW VERSION OF RICASSO?
Extract the new ZIP, start Ricasso.exe there and choose "Reuse previous installation" – or drag the old Ricasso folder onto the window.

This is not legal advice. Follow the terms of use of Swordigo and Google Play.`
