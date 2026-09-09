# Generated output boundary

No ROM-dependent generated code exists in this milestone.

Future translation output belongs in arm/, thumb/, symbols/ and game/. Those directories are ignored except their placeholder files. Each generation must require a validated user ROM, a versioned region manifest and a pinned generator version. Unsupported decoding stops generation; never sweep the ROM assuming every word is code. Do not publish locally generated game code/data with the generic runtime.

The current bounded reference executor lives in src/runtime/probe.cpp. It is handwritten validation infrastructure, not generated code.
