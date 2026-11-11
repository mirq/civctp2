# Plan: PC-hosted games for the Amiga (remote AI)

Goal: speed up turns on the 68060 by running the game host on a PC (Linux or
Windows build). The Amiga joins as a network client and controls the only
human player. All AI civilisations run on the PC.

Status: idea and plan only, nothing implemented. The Amiga builds currently
link `anet_stub.c`, so every ActiveNet call fails and there is no
multiplayer.

## Why it should help

CTP2 multiplayer is host-authoritative. The AI work is gated on
`!g_network.IsClient()`:

- `ai/ctpai.cpp:518`: `AddBeginScheduler` (goal/agent matching, the main
  AI cost)
- `ai/ctpai.cpp:1249`: government choice (`Governor::ComputeBestGovernment`)
- `ai/ctpai.cpp:1269`: strategy and tax/science settings
- `ai/diplomacy/nproposalevent.cpp:116`, `gs/gameobj/Player.cpp:561` and
  similar spots: diplomacy and turn processing on the host only

The client receives the results as packets (`net/general/net_*.cpp`) and
only applies them.

## What still runs on the Amiga

- rendering, UI and sound
- loading the full rules database (memory, start-up time)
- unpacking and applying incoming updates: unit moves, cities, vision,
  terrain. In big games this stream may become the next bottleneck.
- pathfinding and orders for the human player's own units

## Steps

1. **Measure on PCs first (cheap, no Amiga work).**
   Host a game on Linux with several AI civs, join from a second Linux
   instance (same or another machine). Using a `--enable-logging` build,
   measure per turn:
   - how long the client waits for the host's AI turns
   - how much CPU the client spends applying network updates
     (`net/general`, `NetworkManager`/`network.cpp` packet handling)
   - packet volume per turn (useful to estimate the Amiga's Ethernet load)
   Continue only if client CPU time is a small fraction of single-player
   turn time.
   While doing this, check the multiplayer stability of the current code
   (desyncs, crashes): it is the least tested part of the codebase.

2. **Port anet's internet transport to bsdsocket.library.**
   - Replace `os/amiga/anet_stub.c` with a real build of `libs/anet`
     (`dp` core + the UDP/internet driver from `src/`), using
     Roadshow/AmiTCP/Miami headers.
   - anet loads drivers dynamically on Windows; link the one driver
     statically on Amiga.
   - anet was originally cross-platform (including the Mac), so check
     whether its headers are already in network byte order before
     changing anything.
   - Keep all AmigaOS headers in `os/amiga/*.c` (via `amiga_os.h`), per the
     port's rules. Guard everything with `CTP2_AMIGA_BUILD`.
   - Network thread: `net/io/net_thread.cpp` already has Amiga changes;
     check whether a polled (single-task) model is simpler than a second
     Amiga task.

3. **Audit packet byte order and struct layout.**
   - Messages that go through `getlong`/`putlong`/`PUSHLONG`/`PULLLONG`
     are already little-endian on the wire (`net/io/net_util.h`).
   - Find every packet that `memcpy`s a struct, array or raw `sint16`/
     `sint32`/`double` (grep `memcpy`, `PUSHDATA`/`PULLDATA` and similar in
     `net/`) and convert it to field-by-field serialization.
   - The 68k aligns 32-bit fields on 2 bytes, so raw struct copies differ
     in size as well as byte order.
   - Floating-point values: send them as little-endian IEEE bits, as
     `getlong64` already does for doubles.

4. **Fix the sync check (`net/general/net_crc.cpp`).**
   - The database checksums come from `CivArchive` serialization, which
     should already be little-endian (check by comparing the CRCs printed
     on Linux and on the Amiga for the same data).
   - The SLIC checksum hashes `slicif_get_code()` as raw memory, which
     differs on big-endian machines. Either serialize the SLIC code
     endian-neutrally before hashing, or skip that entry when the peers
     have different byte order.

5. **Lock down versions and data.**
   Host and client must use the same source revision and identical
   `ctp2_data` (gamedata, aidata, SLIC scripts). Consider showing a clear
   error on mismatch instead of a generic desync.

6. **Test end to end.**
   - First in an emulator (WinUAE/FS-UAE with bsdsocket emulation) against
     the Linux host, then on the A4000 (Ethernet card + TCP/IP stack).
   - Run the AI regression from CONTRIBUTING.md on the host (`/rnd`) with
     the Amiga attached, and watch for desyncs.
   - Compare turn times with single-player on the 68060 and on WarpOS.

## Risks

- The client update stream may cost more on a 68060 than expected
  (step 1 answers this).
- Hidden raw-memory packets cause desyncs that are hard to find. Add a
  debug mode that logs each packet type and size on both sides so they can
  be diffed.
- Existing multiplayer bugs unrelated to the Amiga.
- Memory: the client still holds the full game state and database.

## Constraints (from the port rules)

- The Linux SDL build must stay unchanged; add Amiga branches only.
- No C++ exceptions on Amiga: guard any throw/catch with `__EXCEPTIONS`.
- Preserve ISO-8859-1 encoding and CRLF line endings in existing sources.
