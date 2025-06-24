# dawdiolab

Terminal-based audio processing tool for SACD extraction, format conversion, and metadata editing.

## Build

```bash
git clone https://github.com/barstoolbluz/dawdiolab.git
cd dawdiolab
make
```

With Flox:
```bash
flox activate
make
```

## Usage

```bash
./dawdiolab [directory]
```

### Interface

Three-pane ncurses TUI:
- Left: File browser
- Middle: Context-dependent (track listing, metadata, queue)
- Right: Settings/progress

### Keys

- `Tab`/`Shift+Tab`: Switch panes
- `Space`: Toggle selection
- `A`/`N`: Select all/none
- `Enter`: Process selection
- `q`: Quit

## Features

### Implemented
- SACD ISO extraction to DSF/DSDIFF
- Track selection interface
- Progress monitoring

### Planned
- Additional format support (FLAC, WAV, DFF, CUE)
- Batch processing
- Metadata editing
- Format conversion via sox-dsd

## Current Architecture

```
libsacd/       SACD format handling
libtui/         TUI framework
ui/             Application components
```

## Dependencies

- ncurses
- pthread
- sox-dsd (external)

## Documentation

- `PROJECT_STATUS_SUMMARY.md`: Project roadmap
- `SESSION_PROGRESS_LOG.md`: Development log

## License

GPL-2.0-or-later
