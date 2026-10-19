Read-only native player inspection
=================================

inspect_native.py reads a Windows x64 PE file from disk. It does not load its
code, attach to Resolve, invoke methods, set breakpoints or modify any binary.
Generated reports belong under build/windows/native-player-inspect/.

Environment used:
  python -m venv build/windows/native-audition-tools
  build/windows/native-audition-tools/Scripts/python.exe -m pip install pefile==2024.8.26 capstone==5.0.9

Examples from the repository root (PowerShell):
  $python = '.\build\windows\native-audition-tools\Scripts\python.exe'
  $binary = 'C:\Program Files\Blackmagic Design\DaVinci Resolve\Resolve.exe'
  & $python tests/platforms/windows/native-player-inspect/inspect_native.py $binary --class-name UiAudioPanelImp
  & $python tests/platforms/windows/native-player-inspect/inspect_native.py $binary --class-name UiPlayerWidgetImp
  & $python tests/platforms/windows/native-player-inspect/inspect_native.py $binary --class-name UiFocusedWidgetManager
  & $python tests/platforms/windows/native-player-inspect/inspect_native.py $binary --disasm 0x144f096d0 --size 0xc6
  & $python tests/platforms/windows/native-player-inspect/inspect_native.py $binary --refs 0x1479d4ff0

The metadata reader understands the Qt 5 x64 QByteArrayData and revision 7/8
metaobject formats. Type IDs use Qt's QMetaType numbering; custom types are
reported by name. Methods are local indices, not indices including superclasses.
The disassembler annotates imports, with raw import-name lookup as a fallback
because pefile does not enumerate all Qt imports in this particular executable.

--refs finds candidate relative call/jump/LEA references inside .text and reports
the enclosing PE unwind-function range. It scans byte patterns, not a recovered
control-flow graph. Verify every candidate by disassembling at a known instruction
boundary; function ranges can also be exception-handling fragments. A disassembly
size is explicit and can run into padding, jump tables or the next function.

All addresses are preferred image VAs for the exact inspected binary, NOT live
addresses and NOT a supported Resolve API. Do not call them or hardcode them into
Unbound. Static evidence is not proof that a branch executes in the failing case.
See doc/issues/fairlight-audition-investigation-2026-10-13.txt for verified findings,
binary version/hash, boundaries of the conclusions and next runtime checkpoints.
