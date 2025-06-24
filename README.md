# SACD Lab TUI - Professional SACD Extraction Tool

A professional terminal UI application for SACD (Super Audio CD) extraction and conversion, written in C using ncurses. This tool extracts **real audio** from SACD ISO files - not dummy files or mock implementations.

## 🎵 Features

- **Real SACD Extraction** - Creates 378MB+ DSF/DSDIFF files (not 8KB dummies)
- **Professional TUI** - Three-pane interface inspired by cmus and Harlequin
- **Track Selection** - Choose specific tracks with green Unicode checkmarks
- **Real-time Progress** - Throttled progress updates during extraction
- **Mouse Support** - Full ncurses mouse integration
- **Self-contained** - Custom libsacd library and libtui framework

## 🏗️ Architecture

- **libsacd** - Self-contained SACD extraction library
- **libtui** - Custom TUI framework with event-driven panes
- **sacd-lab-tui** - Main application combining both

## 🚀 Quick Start

### Using Flox (Recommended)

```bash
# Clone the repository
git clone https://github.com/barstoolbluz/dawdiolab.git
cd dawdiolab

# Activate Flox environment (includes all dependencies)
flox activate

# Build and run
make sacd-lab-tui
./sacd-lab-tui
```

### Manual Setup

Requirements:
- gcc
- make
- ncurses development headers
- pthread support

```bash
# Build
make all

# Test extraction
make test-libsacd
./test_libsacd "path/to/sacd.iso"

# Run TUI
./sacd-lab-tui
```

## 📖 Usage

### Three-Pane Interface

1. **Browser Pane** (Left) - Navigate directories and SACD ISOs
2. **SACD Info Pane** (Middle) - View metadata and select tracks
3. **Progress Pane** (Right) - Monitor extraction progress

### Key Bindings

- `Tab` / `Shift+Tab` - Switch between panes
- `Arrow Keys` - Navigate within panes
- `Space` - Toggle track selection
- `A` - Select all tracks
- `N` - Select no tracks
- `F5` - Start extraction
- `q` - Quit

## 🔧 Development

See [CLAUDE.md](CLAUDE.md) for complete development guidelines and Flox setup.

### Core Files

- `sacd_tui_adapter.c` - Main UI logic and event handling
- `libsacd/` - Real SACD extraction library
- `libtui/` - Custom TUI framework
- `PROJECT_STATUS_SUMMARY.md` - Comprehensive project status
- `SESSION_PROGRESS_LOG.md` - Session-by-session tracking

## ✅ Project Status

- ✅ **Real SACD Parsing** - No more dummy files!
- ✅ **378MB+ File Creation** - Actual audio extraction
- ✅ **Professional TUI** - Three-pane interface working
- ✅ **Track Selection** - Green checkmarks with keyboard navigation
- ✅ **Progress Throttling** - Fixed callback flooding

## 🎯 Philosophy

This project represents a transition from **mock/demo implementations** to **real, working software**. We build tools that extract actual audio, not placeholders.

*"ffs i'm building something i'm going to use. i want something that has real functionality."*

## 📄 License

[Add your license here]

## 🤝 Contributing

1. Read `PROJECT_STATUS_SUMMARY.md` for current state
2. Follow development guidelines in `CLAUDE.md`
3. Update `SESSION_PROGRESS_LOG.md` when making changes
4. Ensure extracted files are 100MB+, not 8KB dummies!