# Third-Party Notices

DL2 Head Tracking bundles or statically links the third-party components listed
below. Each remains the property of its authors and is used under its own
licence. The copyright notices, permission text and disclaimers are reproduced
here verbatim, as those licences require, so that they travel with every binary
we distribute (the GitHub installer ZIP and the Nexus ZIP both carry this file).

Nothing in this repository is derived from, or redistributes any part of, Dying
Light 2 Stay Human. The mod resolves the game's exported symbols and byte
patterns at runtime; no game code, assets or data are copied here or shipped.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| Ultimate ASI Loader | v9.7.2 | MIT | Pre-built binary in the release ZIP |
| MinHook | v1.3.4 | BSD-2-Clause | Compiled into `DL2HeadTracking.asi` |
| Dear ImGui | 1.92.6 WIP (master snapshot, vendored 2026-03-13) | MIT | Compiled into `DL2HeadTracking.asi` |
| Kiero | 1.2.12 | MIT | Compiled into `DL2HeadTracking.asi` |
| inih | r58 | BSD-3-Clause | Compiled into `DL2HeadTracking.asi` |
| EGameTools | research credit (see below) | MIT | Not shipped; one signature derived from it |
| cameraunlock-core | submodule | MIT | Compiled into `DL2HeadTracking.asi` |

---

## Ultimate ASI Loader

Pre-built binary vendored at `vendor/ultimate-asi-loader/dinput8.dll`, shipped
in the installer ZIP and shipped pre-named as `winmm.dll` in the Nexus ZIP.
Extracted untouched from the upstream release asset.

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Tag: `v9.7.2` (commit `ab722befd52581a34449b603926cfab476e66b05`)
- Asset: `Ultimate-ASI-Loader_x64.zip`
- `dinput8.dll` SHA-256: `22fda9c71eaae02460f311bf3441638340ab591586d78f1de213c4819dcb883c`
- Full licence text also ships alongside the binary at
  `vendor/ultimate-asi-loader/LICENSE`.

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## MinHook

Source vendored at `extern/minhook`, compiled into `DL2HeadTracking.asi`.
Version v1.3.4, from https://github.com/TsudaKageyu/minhook.

MinHook carries two copyright holders: Tsuda Kageyu for MinHook itself, and
Vyacheslav Patkov for the Hacker Disassembler Engine that `src/hde/` is derived
from. Both are reproduced below, as they appear in the upstream `LICENSE.txt`.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

---

Portions of this software are derived from Hacker Disassembler Engine:

Hacker Disassembler Engine 32/64
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.
```

---

## Dear ImGui

Source vendored at `extern/imgui`, compiled into `DL2HeadTracking.asi`.
Version 1.92.6 WIP, a master snapshot vendored on 2026-03-13, from
https://github.com/ocornut/imgui. Unmodified.

```
The MIT License (MIT)

Copyright (c) 2014-2025 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Dear ImGui embeds portions of the public-domain `stb` single-file libraries
(`imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h` by Sean Barrett),
which upstream ships under the MIT terms above or, at the recipient's option,
the Unlicense. Those headers carry the original notices verbatim.

---

## Kiero

Source vendored at `extern/kiero`, compiled into `DL2HeadTracking.asi`.
Version 1.2.12, from https://github.com/Rebzzel/kiero. Unmodified.

```
MIT License

Copyright (c) 2014-2021 Rebzzel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## inih

Source vendored at `extern/ini.c` and `extern/ini.h`, compiled into
`DL2HeadTracking.asi`. Version r58, from https://github.com/benhoyt/inih.
Unmodified. The upstream licence file is kept at `extern/LICENSE.inih`.

```
inih -- simple .INI file parser

SPDX-License-Identifier: BSD-3-Clause

Copyright (c) 2009-2024, Ben Hoyt

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Ben Hoyt nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY BEN HOYT ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL BEN HOYT BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## EGameTools

No EGameTools code is shipped or vendored. The byte signature this mod uses to
locate `MoveCameraFromForwardUpPos` in `engine_x64_rwdi.dll`
(`src/hooks/engine_camera_hook.cpp`) is taken from EGameTools' `offsets.h`, so
this mod stands on EricPlayZ's reverse-engineering work and the notice is
reproduced here in full.

- Upstream: https://github.com/EricPlayZ/EGameTools

```
MIT License

Copyright (c) 2023-2024 EricPlayZ

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into `DL2HeadTracking.asi`.
Licensed under the MIT License, copyright (c) 2026 CameraUnlock. Full text at
`cameraunlock-core/LICENSE` and, identically, at `LICENSE` in this repository.

---

## OpenTrack

Not bundled and not linked. This mod implements OpenTrack's UDP pose protocol
so that OpenTrack (https://github.com/opentrack/opentrack, ISC licence) and
compatible trackers can drive it. No OpenTrack code is used.
