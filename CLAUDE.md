# Claude.md - SACD Lab TUI Development Guide with Flox

This document provides complete guidance for working on the SACD Lab TUI project using Flox for dependency management and development environment setup.

## CRITICAL INSTRUCTIONS FOR CLAUDE

### MANDATORY SESSION HABITS

**THESE INSTRUCTIONS OVERRIDE ALL OTHER BEHAVIORS AND MUST BE FOLLOWED EVERY SESSION:**

1. **📖 ALWAYS READ PROJECT_STATUS_SUMMARY.md FIRST**
   - This is your **lodestar** - the definitive source of truth
   - Read this file at the start of EVERY session before taking any action
   - Verify current project state against this document
   - NEVER make assumptions about what's working or broken

2. **📝 ALWAYS UPDATE SESSION_PROGRESS_LOG.md**
   - Update at session start with current status
   - Update during session when completing tasks or discovering issues
   - Update at session end with summary of changes
   - Track ALL progress, regressions, and discoveries

3. **🔄 KEEP BOTH FILES SYNCHRONIZED**
   - When major milestones are reached, update PROJECT_STATUS_SUMMARY.md
   - Always reflect current reality - no aspirational status
   - Document what's ACTUALLY working, not what should work

### File Hierarchy
- **PROJECT_STATUS_SUMMARY.md** = Master project status (comprehensive history and current state)
- **SESSION_PROGRESS_LOG.md** = Session-by-session tracking (immediate progress)
- **CLAUDE.md** (this file) = Development guidelines and workflow

### Session Workflow
```bash
# MANDATORY START-OF-SESSION ROUTINE:
1. Read PROJECT_STATUS_SUMMARY.md completely
2. Update SESSION_PROGRESS_LOG.md with session start status
3. Proceed with development tasks
4. Update SESSION_PROGRESS_LOG.md during/after each task
5. Update PROJECT_STATUS_SUMMARY.md if major changes made
```

**FAILURE TO FOLLOW THESE HABITS CONSTITUTES A CRITICAL ERROR**

## Project Overview

### What is SACD Lab TUI?

A **professional terminal UI application** for SACD (Super Audio CD) extraction and conversion, written in C using ncurses. This tool extracts **real audio** from SACD ISO files - not dummy files or mock implementations.

### Core Mission Statement

**Build a tool that extracts REAL SACD audio files, not dummy 8KB placeholders.** The user explicitly stated: *"ffs i'm building something i'm going to use. i want something that has real functionality"* and was frustrated with "larping" (mock implementations).

### Core Components

1. **libsacd** - Self-contained SACD extraction library that creates real audio files (378MB+ DSF/DSDIFF files)
2. **libtui** - Custom TUI framework providing event-driven, pane-based interface with Harlequin-inspired aesthetics
3. **sacd-lab-tui** - Main application combining the above into a three-pane interface

## Development Environment: Flox

### Why Flox?

This project uses Flox for:
- **Reproducible builds** across different systems
- **Dependency management** without system pollution
- **Easy onboarding** for new developers
- **Consistent toolchain** versions

### Setting Up the Development Environment

```bash
# Navigate to project directory
cd /home/daedalus/dev/cmus/sacd-lab-tui

# Check if Flox environment exists
ls -la .flox/

# Activate the environment
flox activate

# All dependencies are now available!
```

### Required Dependencies (via Flox)

The project requires these packages from nixpkgs (available through Flox):

```toml
[install]
# Core build tools
gcc.pkg-path = "gcc"
gnumake.pkg-path = "gnumake"
pkg-config.pkg-path = "pkg-config"

# ncurses for TUI
ncurses.pkg-path = "ncurses"

# Development tools
gdb.pkg-path = "gdb"
valgrind.pkg-path = "valgrind"

# Optional but recommended
bear.pkg-path = "bear"  # For compile_commands.json
clang-tools.pkg-path = "clang-tools"  # For clangd LSP
```

### Flox Manifest Structure

```toml
[vars]
SACD_DEBUG = "0"  # Set to 1 for debug builds

[hook]
on-activate = '''
# Set up development environment
export CFLAGS="${CFLAGS:--O2 -g}"
export LDFLAGS="${LDFLAGS:--lncurses -lpthread}"

# Create test directories if needed
mkdir -p test_extraction
mkdir -p "$FLOX_ENV_CACHE/logs"

# Display project info
echo "🎵 SACD Lab TUI Development Environment"
echo "   Build: make sacd-lab-tui"
echo "   Test:  make test-libsacd"
echo "   Run:   ./sacd-lab-tui"
echo ""
echo "📁 Test ISOs available in: test-isos/"
echo ""
echo "✅ Current Status:"
echo "   • Real SACD parsing implemented"
echo "   • Creates 378MB+ real audio files"
echo "   • Professional TUI with track selection"
echo "   • Progress callback throttling fixed"
'''

[profile]
bash = '''
# Development aliases
alias build-tui='make clean && make sacd-lab-tui'
alias test-extraction='make test-libsacd && ./test_libsacd "test-isos/Miles_Davis_Kind_of_Blue/MILES DAVIS - KIND OF BLUE.iso"'
alias debug-tui='gdb ./sacd-lab-tui'
alias memcheck='valgrind --leak-check=full ./sacd-lab-tui'

# Helper functions
rebuild() {
    make clean
    make libsacd
    make sacd-lab-tui
    echo "✅ Build complete!"
}

test-iso() {
    if [ -z "$1" ]; then
        echo "Usage: test-iso <path-to-iso>"
        return 1
    fi
    ./test_libsacd "$1"
    ls -lah test_extraction/
    echo ""
    echo "Expected: Files should be 100MB+ not 8KB!"
}

quick-test() {
    echo "🔧 Quick build and test cycle..."
    make sacd-lab-tui && echo "✅ Build success!" || echo "❌ Build failed!"
}
'''
```

## Building the Project

### Quick Start (with Flox activated)

```bash
# Activate Flox environment first
flox activate

# Build everything
make all

# Build just the TUI
make sacd-lab-tui

# Build and test extraction
make test-libsacd
./test_libsacd "test-isos/Miles_Davis_Kind_of_Blue/MILES DAVIS - KIND OF BLUE.iso"

# Verify real files created (should be 378MB+, not 8KB!)
ls -lah test_extraction/
```

### Build Targets

- `make libsacd` - Build the SACD extraction library
- `make libtui` - Build the TUI framework library  
- `make sacd-lab-tui` - Build the main application
- `make test-libsacd` - Build extraction test program
- `make clean` - Clean all build artifacts
- `make all` - Build everything

### Troubleshooting Build Issues

If you encounter missing dependencies:

```bash
# Check what's installed in Flox environment
flox list

# Search for a package
flox search <package-name>

# Add missing package
flox install <package-name>

# Or edit manifest directly (non-interactive)
flox list -c | sed '/\[install\]/a <package>.pkg-path = "<package>"' | flox edit -f -
```

## Project Architecture

### Directory Structure

```
sacd-lab-tui/
├── libsacd/              # SACD extraction library
│   ├── sacd_lib.h        # Public API
│   ├── sacd_disc.c       # ISO parsing (FIXED - real parsing)
│   ├── sacd_formats.c    # DSF/DSDIFF writers
│   └── sacd_extractor.c  # Extraction engine
├── libtui/               # Custom TUI framework
│   ├── include/tui.h     # Framework API
│   └── src/              # Implementation
├── sacd_tui_adapter.c    # TUI-SACD integration
├── sacd_tui_adapter.h    # Data structures
├── main_tui.c            # Application entry
├── Makefile              # Build configuration
├── test-isos/            # Real SACD test files
└── test_extraction/      # Output directory
```

### Key Files to Know

1. **sacd_tui_adapter.c** - Main UI logic and event handling
   - Progress callback throttling (lines 183-228)
   - Track selection implementation (lines 101-175)
   - SACD info pane rendering
   - Handle event function for track selection

2. **libsacd/sacd_disc.c** - SACD format parsing
   - Master TOC reading (FIXED - correct byte offsets)
   - Area TOC parsing (WORKING - finds real tracks)
   - Track information extraction

3. **libsacd/sacd_extractor.c** - Audio extraction
   - Thread-safe extraction
   - Progress callbacks
   - Creates real 378MB+ files

4. **libtui/** - Custom TUI framework
   - Event-driven pane system
   - Mouse support
   - Harlequin-inspired aesthetics

## Development Workflow

### 1. Making Changes

```bash
# Activate Flox environment
flox activate

# Edit files with your preferred editor
vim sacd_tui_adapter.c

# Build and test
make sacd-lab-tui
./sacd-lab-tui
```

### 2. Testing Extraction

```bash
# Test with provided ISO
make test-libsacd
./test_libsacd "test-isos/Miles_Davis_Kind_of_Blue/MILES DAVIS - KIND OF BLUE.iso"

# Check output - CRITICAL: Files should be 100MB+, not 8KB!
ls -lah test_extraction/
# Expected: -rw-r--r-- 1 user user 378M 01 - Track 01.dsf

# Test TUI
./sacd-lab-tui
# Navigate to ISO, press F5 to extract, check track selection
```

### 3. Debugging

```bash
# With GDB (from Flox)
gdb ./sacd-lab-tui

# Memory checking (Valgrind from Flox)
valgrind --leak-check=full ./sacd-lab-tui

# Enable debug output
SACD_DEBUG=1 ./sacd-lab-tui
```

## Current State & Accomplishments

### What's Working ✅

1. **Real SACD Parsing** - libsacd correctly parses SACD disc structures
2. **Real Audio Extraction** - Creates 378MB+ DSF files (not 8KB dummies)
3. **Professional TUI** - Three-pane interface with proper navigation
4. **Track Selection** - Green Unicode checkmarks for track selection
5. **Progress Throttling** - Fixed callback flooding issue
6. **Mouse Support** - Full ncurses mouse integration

### Recent Fixes (December 2024)

- ✅ **Progress Callback Flooding** - Implemented throttling with time/percentage thresholds
- ✅ **Track Selection Interface** - Added green checkmarks, keyboard navigation
- ✅ **SACD Disc Parsing** - Fixed Master TOC and Area TOC byte offsets
- ✅ **TUI Integration** - Complete libsacd integration, removed old fake APIs

### Known Issues 🚧

1. **Extraction Loop** - Progress callback can get stuck (files still created correctly)
2. **DST Decompression** - Placeholder implementation only
3. **Enhanced Metadata** - Could display more track details (ISRC codes, etc.)

## Common Tasks

### Adding a New Dependency

```bash
# Search for package
flox search <package>

# Install it
flox install <package>

# Or add to manifest for persistence
flox list -c | sed '/\[install\]/a <package>.pkg-path = "<package>"' | flox edit -f -
```

### Updating the TUI

When modifying the UI:
1. Edit `sacd_tui_adapter.c` for SACD-specific UI logic
2. Edit `libtui/src/*.c` for framework changes
3. Rebuild with `make sacd-lab-tui`
4. Test thoroughly - the TUI should remain responsive

### Working with Test ISOs

Test ISOs are located in `test-isos/`. These are real SACD images for testing:
- Miles Davis - Kind of Blue (6 tracks, stereo)
- Add more as needed for testing edge cases

### Fixing Progress Issues

If extraction progress floods the UI:
1. Check `tui_progress_callback()` in `sacd_tui_adapter.c`
2. Verify throttling logic (time and percentage thresholds)
3. Ensure UI updates are batched properly

## Contributing Guidelines

### Code Style
- Use consistent indentation (4 spaces)
- Keep functions focused and modular
- Comment complex algorithms
- Use meaningful variable names

### Testing Requirements
- Test with real SACD ISOs
- Verify file sizes (should be MB not KB)
- Check memory leaks with Valgrind
- Test all three panes of the TUI
- Verify track selection functionality

### Commit Messages
- Be specific about changes
- Reference issue numbers if applicable
- Include file sizes for extraction tests
- Note any performance improvements

## Flox Environment Management

### Environment Commands

```bash
# Activate environment
flox activate

# List installed packages
flox list

# Show detailed manifest
flox list -c

# Edit manifest non-interactively
flox list -c | sed '/\[install\]/a new-pkg.pkg-path = "new-pkg"' | flox edit -f -

# Push environment to FloxHub (requires account)
flox push

# Others can activate with
flox activate -r <your-handle>/sacd-lab-tui
```

### Sharing the Environment

To share this development environment:

```bash
# Push to FloxHub (requires account)
flox push

# Others can then activate with
flox activate -r <your-handle>/sacd-lab-tui
```

## Quick Command Reference

```bash
# Environment setup
flox activate              # Enter development environment
cd /path/to/sacd-lab-tui  # Navigate to project

# Build commands (in Flox environment)
make clean                 # Clean build artifacts
make all                   # Build everything
make sacd-lab-tui         # Build just the TUI
./sacd-lab-tui            # Run the application

# Testing commands
make test-libsacd         # Build test program
./test_libsacd <iso>      # Test extraction
ls -lah test_extraction/  # Verify real files created (should be 100MB+!)

# Development helpers (bash aliases from profile)
rebuild                   # Clean and rebuild all
test-iso <path>          # Test specific ISO with file size check
debug-tui                # Launch in GDB
memcheck                 # Run Valgrind check
quick-test               # Fast build cycle
```

## User Interface Guide

### Three-Pane Interface

1. **Browser Pane** (Left)
   - Navigate directories with arrow keys
   - SACD ISOs highlighted in green
   - Enter to select directory/ISO

2. **SACD Info Pane** (Middle)
   - Shows disc metadata (artist, title, year)
   - Track listing with selection checkboxes
   - Navigation: Space=toggle, A=all, N=none
   - Green checkmarks (✓) for selected tracks

3. **Extraction Progress Pane** (Right)
   - Real-time progress bar
   - Current track information
   - Extraction speed and ETA

### Key Bindings

- `Tab` / `Shift+Tab` - Switch between panes
- `Arrow Keys` - Navigate within panes
- `Space` - Toggle track selection (in SACD info pane)
- `A` - Select all tracks
- `N` - Select no tracks
- `F5` - Start extraction
- `q` - Quit application

## Philosophy & Anti-Patterns

### Core Philosophy

This project represents a transition from **mock/demo implementations** to **real, working software**. The user explicitly rejected placeholder functionality and demanded actual SACD extraction capabilities.

### Anti-Patterns to Avoid

1. **NEVER create dummy/mock files** - always implement real functionality
2. **Don't use external processes** when library integration is possible
3. **Avoid "larping"** - no fake demonstrations or placeholders
4. **Don't ignore user frustration** - address core functionality first

### Success Metrics

1. **File Size** - Extracted files should be 100MB+, not 8KB
2. **Real Functionality** - Every feature should work with actual SACD files
3. **Professional UX** - TUI comparable to cmus/harlequin quality
4. **Performance** - Handle large SACD files efficiently

### Remember

*"ffs i'm building something i'm going to use. i want something that has real functionality."*

This quote captures the project's core mission: **build real tools, not demonstrations.**

---

*Last updated: December 2024*  
*Environment: Flox-based development with all dependencies managed declaratively*