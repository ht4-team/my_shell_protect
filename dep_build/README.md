# dep_build

This directory contains downloadable + rebuildable outputs for:

- `capstone` (tag `4.0.2`)
- `lz4` (tag `v1.9.2`)

## Build

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\dep_build\build_deps.ps1
```

## Output layout

- `dep_build/capstone/32`
- `dep_build/capstone/64`
- `dep_build/capstone/include`
- `dep_build/lz4/static`
- `dep_build/lz4/dll`
- `dep_build/lz4/include`
- `dep_build/lz4/example`

The directory/file names are aligned with the current prebuilt tree so you can replace by folder.

Note: `dep_build/lz4/dll/liblz4.dll.a` is generated as a filename-compatible copy of `liblz4.lib`.
