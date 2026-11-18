# Third-party components

- MGSM upstream: https://github.com/LeithSilver/mgsm — inherited GNU GPL license retained in LICENSE. This adaptation retains the MGSM-derived runtime and the subsequent separate Undercover compatibility work.
- PL_MPEG by Dominic Szablewski: MIT license retained in third_party/pl_mpeg.h; pinned revision c871f2be022ece7ef4f64230b4fb8e1fb9eb6023. The decoder header is unmodified.
- Movie player wrapper: third_party/movie_player_LICENSE.
- LZMA SDK: included implementation and notices under third_party/lzma/.
- mpg123: dynamically unsupported on this platform; linked through the devkitPro 3ds-mpg123 port. Original AUTHORS, COPYING and the 1.31.3 source archive are retained under third_party/mpg123/. Use the matching port library when rebuilding.
- devkitARM/libctru/zlib: external build dependencies, supplied by devkitPro; retain their applicable notices when redistributing their components.

Pandemonium game executable/resources and converted movies must be prepared from the user's own supported package. They are excluded from both public archives. Artwork supplied for this adaptation is included for the port; no independent ownership or blanket code-license grant over game branding is asserted.
