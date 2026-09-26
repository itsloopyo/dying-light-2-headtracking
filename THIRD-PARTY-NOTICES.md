# Third-Party Notices

DL2 Head Tracking bundles or statically links the third-party components listed
below. Each remains the property of its authors and is used under its own
licence. The copyright notices, permission text and disclaimers are reproduced
here verbatim, as those licences require, so that they travel with every binary
we distribute (the GitHub installer ZIP and the Nexus ZIP both carry this file).

No game code, no extracted assets and no data files from Dying Light 2 Stay
Human are contained here. The mod resolves the game's exported symbols and byte
patterns at runtime. The one piece of Techland's work this repository does carry
is the README demo clip, covered in its own section at the end of this file.

| Component | Version | Licence | How it ships | Modified by us |
|-----------|---------|---------|--------------|----------------|
| Ultimate ASI Loader | v9.7.2 | MIT | Pre-built binary, installer ZIP only | No |
| injector | `f7fd18f` (inside Ultimate ASI Loader v9.7.2) | zlib | Compiled into the vendored dinput8.dll | No |
| miniz | 3.0.0 (inside Ultimate ASI Loader v9.7.2) | MIT | Compiled into the vendored dinput8.dll | No |
| MinHook | v1.3.4 | BSD-2-Clause | Compiled into `DL2HeadTracking.asi` | Yes, see below |
| Dear ImGui | 1.92.6 WIP | MIT | Compiled into `DL2HeadTracking.asi` | Yes, see below |
| Kiero | 1.2.12 | MIT | Compiled into `DL2HeadTracking.asi` | Yes, see below |
| inih | r58 | BSD-3-Clause | Compiled into `DL2HeadTracking.asi` | No |
| cameraunlock-core | `3f3a821aa00d5b87ecde7c4af77585ff2fc71ff5` | MIT | Compiled into `DL2HeadTracking.asi` | Our own code |
| EGameTools | research credit | MIT | Not shipped; one signature derived from it | n/a |
| OpenTrack | n/a | ISC | Not bundled; UDP wire format only | n/a |

Where a component is marked modified, the change is ours, is described in that
component's section below, and is permitted by its licence. It is recorded so
the attribution is not mistaken for a claim of an unmodified copy.

---

## Ultimate ASI Loader

Pre-built binary vendored at `vendor/ultimate-asi-loader/dinput8.dll`, extracted
untouched from the upstream release asset and shipped in the installer ZIP only.
The Nexus ZIP does not bundle it: redistributing another author's tool through a
Nexus upload is not ours to do, so it is a stated requirement there and users
install it themselves.

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

That `dinput8.dll` is a static binary and is not one component. The
`Ultimate-ASI-Loader-x64` target in `premake5.lua` at v9.7.2 compiles
`external/injector/minhook/src/**.c`,
`external/injector/utility/FunctionHookMinHook.cpp` and `external/miniz/miniz.c`
alongside the loader's own sources, so redistributing it redistributes MinHook,
injector and miniz as well, and each has its own section in this file.
MemoryModule, d3d8to9 and the minidx9 DirectX headers belong to the 32-bit
target only and are absent from this binary. The MinHook section covers the copy
inside the loader as well as any linked into the mod itself; the licence text is
the same.

---

## injector

Compiled into the vendored `dinput8.dll`. The loader's `FunctionHookMinHook`
wrapper, which the `Ultimate-ASI-Loader-x64` target compiles from
`external/injector/utility/FunctionHookMinHook.cpp`, and the MinHook submodule
that repository carries. Nothing in this repository calls or links it; it ships
only inside that binary.

- Upstream: https://github.com/ThirteenAG/injector
- Version: commit `f7fd18f7fcb4691f470b7a047697e591a39a94fc`, the submodule
  Ultimate ASI Loader v9.7.2 pins at `external/injector/`
- Licence: zlib

The binary is unaltered upstream, so the "altered source versions" condition
below does not arise. It is reproduced whole regardless.

```
Copyright (C) 2012-2014 LINK/2012 <dma_2012@hotmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## miniz

Compiled into the vendored `dinput8.dll`. Zip reading for the loader's
`LoadVirtualFilesFromZip` path, which the `Ultimate-ASI-Loader-x64` target
compiles from `external/miniz/miniz.c`. Nothing in this repository calls or
links it; it ships only inside that binary.

- Upstream: https://github.com/richgel999/miniz
- Version: 3.0.0, as vendored at `external/miniz/` in Ultimate ASI Loader v9.7.2
- Licence: MIT

```
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## MinHook

Source vendored at `extern/minhook`, compiled into `DL2HeadTracking.asi`.
Version v1.3.4, from https://github.com/TsudaKageyu/minhook.

MinHook carries two copyright holders: Tsuda Kageyu for MinHook itself, and
Vyacheslav Patkov for the Hacker Disassembler Engine that `src/hde/` is derived
from. Both are reproduced below, as they appear in the upstream `LICENSE.txt`,
which also ships verbatim at `extern/minhook/LICENSE.txt`.

Our modification is confined to `src/hook.c`: `MH_Initialize` takes the process
heap via `GetProcessHeap()` instead of standing up a private heap with
`HeapCreate`, so `MH_Uninitialize` skips the matching `HeapDestroy`. Every other
file under `extern/minhook` is byte-for-byte upstream v1.3.4.

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

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
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
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
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
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## Dear ImGui

Source vendored at `extern/imgui`, compiled into `DL2HeadTracking.asi`.
Version 1.92.6 WIP, from https://github.com/ocornut/imgui, taken at upstream
commit `922a11f0847fe1a7907345c31a22e0e77a43829c` (2025-12-23).

Our modification is confined to `imgui_impl_dx12.cpp`, one added line,
`IM_UNUSED(hr);`, silencing an unused-variable warning. Every other vendored
ImGui file is byte-for-byte upstream at the commit above.

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
Version 1.2.12, from https://github.com/Rebzzel/kiero. The upstream licence file
also ships verbatim at `extern/kiero/LICENSE`.

Our modifications:

- `kiero.h`: the upstream feature switches `KIERO_INCLUDE_D3D11`,
  `KIERO_INCLUDE_D3D12` and `KIERO_USE_MINHOOK` are turned on. These exist to be
  set by the consumer.
- `kiero.cpp`: includes `<dxgi1_4.h>` rather than `<dxgi.h>`, and `"MinHook.h"`
  rather than `"minhook/include/MinHook.h"` to match this project's include
  paths; and the D3D12 method table grows from 150 to 172 entries, filled from
  an `IDXGISwapChain3` query so `Present1` can be hooked.

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
Unmodified, byte-for-byte upstream. The upstream licence file is kept verbatim
at `extern/LICENSE.inih`.

```
The "inih" library is distributed under the New BSD license:

Copyright (c) 2009, Ben Hoyt
All rights reserved.

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
that signature is EricPlayZ's work and the notice is reproduced here in full.

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
Our own code, MIT licensed, reproduced here so the notices are complete.

- Upstream: https://github.com/itsloopyo/cameraunlock-core
- Pinned commit: `3f3a821aa00d5b87ecde7c4af77585ff2fc71ff5`

```
MIT License

Copyright (c) 2026 itsloopyo

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

## OpenTrack

Not bundled and not linked. This mod implements the OpenTrack UDP pose datagram
layout so that OpenTrack (https://github.com/opentrack/opentrack, ISC licence)
and compatible trackers can drive it. No OpenTrack code, headers or binaries are
copied, linked or redistributed, so its licence triggers no notice obligation
here. It is credited because the wire format is its work.

---

## Dying Light 2 Stay Human

Dying Light 2 Stay Human and all related names, logos, characters and marks are
trademarks of Techland. They are used here only to identify the game this mod
applies to, which is nominative use and not a claim of any right in them.

This project is an unofficial, fan-made modification. It is not affiliated with,
endorsed by, or sponsored by Techland, its publishers, its engine vendor, or any
other rights holder.

It redistributes no game code, no extracted game assets and no proprietary DLLs,
and it requires a legitimately purchased copy of the game.

### The README demo clip

`assets/readme-clip.gif` is a short capture of ordinary Dying Light 2 gameplay,
recorded by us to show what the mod does at the top of the README. The footage,
the HUD and everything visible in it are the copyrighted audiovisual work of
Techland and its publishers. The MIT licence covering our own code does not
extend to it, and no ownership of it is claimed.

It is kept for the same reason every mod page carries a clip: it is the only
thing that shows a visitor what the mod actually does. It lives in the
repository only. The packaging script copies `README.md`, `LICENSE`,
`CHANGELOG.md` and this file and never `assets/`, so the clip ships in neither
the installer ZIP nor the Nexus ZIP. The README references it by absolute URL,
so it still renders for anyone reading the copy inside a ZIP.

We will remove it on request from Techland or its publishers.

The engine structure offsets, exported symbol names and byte patterns referenced
in the source were derived by the authors through independent analysis of a
legitimately owned copy, or credited to EGameTools above. They are factual
measurements recorded as numbers; no game code of any kind is stored in this
repository.
