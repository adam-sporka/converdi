<p align="center">
  <img src="converdi.png" alt="Converdi">
</p>

**Converdi** is an open-source tool for composers who write music in **Sibelius** and perform it with **sample libraries** in a DAW. It converts a Sibelius score into a DAW-ready MIDI file that accounts for articulations, dynamics, playing techniques, and sample library routing — things Sibelius's built-in MIDI export silently discards.

---

## The problem it solves

When you export MIDI from Sibelius, you get notes and tempo. Every note sounds the same regardless of whether it is slurred, staccato, tremolo, or pizzicato. All dynamics are ignored. The velocity is flat. Sample library keyswitch logic is nonexistent.

Getting a realistic MIDI mockup from a Sibelius score currently means manually re-entering everything in the DAW — a tedious, error-prone process that breaks down every time the score changes.

Converdi automates this translation.

---

## How it works

The workflow has two steps.

**Step 1 — Export from Sibelius**

Install `Converdi.plg` as a Sibelius plugin (*Plugins > Gather Data for Converdi*). Select a passage or the whole score and run it. The plugin produces a text dump alongside your score file, named `<score>-dump.txt`. This dump captures everything Sibelius knows about the score: note positions (tuplet-corrected), sounding and written pitch, articulation flags, tremolo, arpeggio direction, ties, slurs, trills, dynamics, pedal lines, tempo and ritardando markings, time and key signatures, and voice assignments.

**Step 2 — Convert to MIDI**

Run `converdi.exe` from the command line:

```
converdi <score>-dump.txt <profile-name>
```

Converdi reads the dump and a **profile** (an `.ini` file in the `data/profiles/` folder) that maps your instrument staves to **presets** (further `.ini` files in `data/presets/`). Each preset describes how to route notes for a specific instrument in a specific sample library: which notes belong to which articulation track, how to inject CC automation for dynamics, how to shape note durations, whether to insert keyswitch notes, and more.

The output is a `.mid` file ready to load into your DAW.

---

## Configuration

Profiles and presets are plain `.ini` text files — no GUI required. A profile lists the staves in your score and which preset to apply to each. A preset can:

- Route notes to separate MIDI tracks by articulation (slur/legato, staccato, tremolo, pizzicato, trill, or MIDI pitch range)
- Inject CC automation derived from written dynamics (e.g. CC1 or CC11)
- Insert keyswitch notes at region boundaries with configurable anticipation and prolong timing
- Shape note durations (shorten, prolong, or arpeggiate)
- Map pedal lines to CC64
- Transpose or force notes to a specific pitch or velocity

Presets for **Barbora**, **CineSamples**, and **ERA2** sample libraries are included as a starting point.

---

## Building

The repository includes a Visual Studio 2022 solution (`converdi.sln`). Open it and build. The only external dependency is [midifile](https://github.com/craigsapp/midifile) (BSD licensed) by Craig Stuart Sapp. Clone or download it and place the contents of its `include/` and `src/` directories into `third_party/midifile/` alongside the solution, then add the midifile `.cpp` source files to the project (or build them as a static library).

Requirements:
- Visual Studio 2022 (v143 toolset)
- Windows x64
- C++17

---

## Repository layout

```
converdi/
├── Converdi.plg          # Sibelius ManuScript plugin (Step 1)
├── converdi.sln          # Visual Studio 2022 solution
├── converdi.vcxproj
├── src/                  # C++ source
└── data/
    ├── profiles/         # Score-level configuration (.ini)
    └── presets/          # Per-library instrument presets (.ini)
        ├── CineSamples/
        ├── ERA2/
        ├── Game/         # Presets by Jan Valta we used for Kingdom Come: Deliverance II
        └── Test/
```

---

## License

Converdi is released under the **Apache License 2.0 with Commons Clause**.

You are free and welcome to use, study, modify, and distribute Converdi — including as part of a commercial production workflow. The Commons Clause addendum adds one restriction: **you may not sell Converdi, or a product whose value is substantially derived from it**, without explicit written permission from the author.

In plain terms:
- ✅ Use it in your studio's pipeline
- ✅ Use it as part of paid composition or production work
- ✅ Modify it and share your changes
- ✅ Build presets and contribute them back
- ❌ Package and sell it as a product
- ❌ Offer it as a paid tool or service to others

The full Apache 2.0 license text is available at [https://www.apache.org/licenses/LICENSE-2.0](https://www.apache.org/licenses/LICENSE-2.0). The Commons Clause is available at [https://commonsclause.com](https://commonsclause.com).

For commercial licensing enquiries, open an issue on GitHub.

---

## Join the community

Converdi is a small tool solving a real problem, and there is a lot of room to grow. If you are a composer, developer, or both, contributions of any size are welcome.

Areas where help would make the biggest difference:

- **New presets** for other sample libraries (Spitfire, OT, EastWest, NotePerformer, etc.)
- **Parser improvements** — the Sibelius dump format has edge cases that are not yet fully handled
- **Cross-platform build** — the codebase is almost portable; the main blocker is the `<conio.h>` dependency and `fopen_s` usage in `converdi.cpp`
- **MusicXML input** — an alternative input path would make Converdi usable from Finale, Dorico, and MuseScore
- **Bug fixes** — see the known issues in the source

If you want to get involved, open an issue or start a discussion on GitHub. All skill levels welcome — even just sharing presets for a library you own is a valuable contribution.
