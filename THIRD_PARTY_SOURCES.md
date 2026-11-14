# Third-party sources

The Windows package bundles the runtime libraries below as separate DLLs, built by the MSYS2 project.
Each link is the exact source package of the shipped version. Their license texts are in the package under
`app/licenses/dependencies/`, and `tools/windows/packages.lock.json` fixes the versions and SHA-256 hashes of the whole toolchain.

| Package | Version | License | Files | Source |
| --- | --- | --- | --- | --- |
| brotli | 1.2.0-1 | MIT | libbrotlicommon.dll, libbrotlidec.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-brotli-1.2.0-1.src.tar.zst) |
| bzip2 | 1.0.8-4 | custom | libbz2-1.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-bzip2-1.0.8-4.src.tar.zst) |
| double-conversion | 3.4.0-1 | BSD-3-Clause | libdouble-conversion.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-double-conversion-3.4.0-1.src.tar.zst) |
| freetype | 2.15.0-1 | GPL-2.0-or-later OR FTL | libfreetype-6.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-freetype-2.15.0-1.src.tar.zst) |
| gettext-runtime | 1.0-1 | GPL-3.0-or-later AND LGPL-2.1-or-later | libintl-8.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-gettext-1.0-1.src.tar.zst) |
| glew | 2.3.1-4 | Modified BSD/MIT/GPL | glew32.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-glew-2.3.1-4.src.tar.zst) |
| glib2 | 2.90.1-1 | LGPL-2.1-or-later | libglib-2.0-0.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-glib2-2.90.1-1.src.tar.zst) |
| graphite2 | 1.3.15-1 | LGPL-2.1-or-later | libgraphite2.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-graphite2-1.3.15-1.src.tar.zst) |
| harfbuzz | 14.6.0-1 | MIT | libharfbuzz-0.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-harfbuzz-14.6.0-1.src.tar.zst) |
| icu | 78.3-4 | ICU | libicudt78.dll, libicuin78.dll, libicuuc78.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-icu-78.3-4.src.tar.zst) |
| libb2 | 0.98.1-3 | CC0 | libb2-1.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-libb2-0.98.1-3.src.tar.zst) |
| libgcc | 16.2.0-6 | GPL-3.0-or-later WITH GCC-exception-3.1 AND GFDL-1.3-or-later | libgcc_s_seh-1.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-gcc-16.2.0-6.src.tar.zst) |
| libiconv | 1.19-1 | LGPL-2.1-or-later, docs: GPL-3.0-or-later | libiconv-2.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-libiconv-1.19-1.src.tar.zst) |
| libogg | 1.3.6-1 | BSD-3-Clause | libogg-0.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-libogg-1.3.6-1.src.tar.zst) |
| libpng | 1.6.59-1 | custom | libpng16-16.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-libpng-1.6.59-1.src.tar.zst) |
| libstdc++ | 16.2.0-6 | GPL-3.0-or-later WITH GCC-exception-3.1 AND GFDL-1.3-or-later | libstdc++-6.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-gcc-16.2.0-6.src.tar.zst) |
| libvorbis | 1.3.7-3 | custom | libvorbis-0.dll, libvorbisfile-3.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-libvorbis-1.3.7-3.src.tar.zst) |
| libwinpthread | 14.0.0.r495.g63ebe6cdb-1 | MIT AND BSD-3-Clause | libwinpthread-1.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-winpthreads-14.0.0.r495.g63ebe6cdb-1.src.tar.zst) |
| md4c | 0.6.0-1 | MIT | libmd4c.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-md4c-0.6.0-1.src.tar.zst) |
| mpg123 | 1.33.7-1 | LGPL-2.1-or-later | libmpg123-0.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-mpg123-1.33.7-1.src.tar.zst) |
| openal | 1.25.2-1 | GPL-2.0-or-later | libopenal-1.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-openal-1.25.2-1.src.tar.zst) |
| pcre2 | 10.49-1 | BSD-3-Clause | libpcre2-16-0.dll, libpcre2-8-0.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-pcre2-10.49-1.src.tar.zst) |
| qt6-base | 6.11.2-2 | LGPL-3.0-only WITH Qt-GPL-exception-1.0 AND AFL-2.1 AND Apache-2.0 AND BSL-1.0 AND CC0-1.0 AND BSD-3-Clause AND CC-BY-4.0 AND GFDL-1.3-no-invariants-only AND GPL-2.0-only AND GPL-2.0-or-later AND GPL-3.0-only AND custom | Qt6Core.dll, Qt6Gui.dll, Qt6Network.dll, Qt6Widgets.dll, qwindows.dll, qmodernwindowsstyle.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-qt6-base-6.11.2-2.src.tar.zst) |
| sdl3 | 3.4.18-1 | Zlib | SDL3.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-sdl3-3.4.18-1.src.tar.zst) |
| sdl3-image | 3.4.8-1 | Zlib | SDL3_image.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-sdl3-image-3.4.8-1.src.tar.zst) |
| zlib | 1.3.2-2 | Zlib | zlib1.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-zlib-1.3.2-2.src.tar.zst) |
| zstd | 1.5.7-2 | BSD-3-Clause OR GPL-2.0-or-later | libzstd.dll | [source](https://repo.msys2.org/mingw/sources/mingw-w64-zstd-1.5.7-2.src.tar.zst) |

Build recipes for these packages: https://github.com/msys2/MINGW-packages

Source code compiled into `swordfare.exe` and the guest libraries (Dynarmic and its dependencies, Dear ImGui, stb, Lua, LuaSocket and others) is part of this repository.
