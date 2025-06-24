# Claude.md - DAWdioLab Development Guide

This document provides complete guidance for working on the DAWdioLab project using Flox for dependency management and development environment setup.

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

### What is DAWdioLab?

A **comprehensive terminal-based audio processing application** for SACD extraction, format conversion, metadata editing, and batch processing. Written in C using ncurses. This tool processes **real audio** - no dummy files or mock implementations.

### Repository

https://github.com/barstoolbluz/dawdiolab

### Core Mission Statement

**Build a tool that processes REAL audio files with professional functionality.** The user explicitly stated: *"ffs i'm building something i'm going to use. i want something that has real functionality"* and was frustrated with "larping" (mock implementations).

### Core Components

1. **libaudio** - Unified audio processing library (evolving from libsacd)
   - Real SACD extraction (378MB+ DSF/DSDIFF files)
   - Multi-format support (planned: FLAC, WAV, DFF, CUE)
   - Metadata handling
   - Conversion coordination

2. **libtui** - Custom TUI framework providing event-driven, pane-based interface

3. **dawdiolab** - Main application with adaptive three-pane interface

## Development Environment: Flox

### Setting Up the Development Environment

```bash
# Clone the repository
git clone https://github.com/barstoolbluz/dawdiolab.git
cd dawdiolab

# Activate the Flox environment
flox activate

# All dependencies are now available!
```

### Required Dependencies (via Flox)

```toml
[install]
# Core build tools
gcc.pkg-path = "gcc"
gnumake.pkg-path = "gnumake"
pkg-config.pkg-path = "pkg-config"

# ncurses for TUI
ncurses.pkg-path = "ncurses"

# Audio libraries (as available)
flac.pkg-path = "flac"
libsndfile.pkg-path = "libsndfile"

# Development tools
gdb.pkg-path = "gdb"
valgrind.pkg-path = "valgrind"
bear.pkg-path = "bear"  # For compile_commands.json
clang-tools.pkg-path = "clang-tools"  # For clangd LSP
```

## Building the Project

### Quick Start (with Flox activated)

```bash
# Activate Flox environment first
flox activate

# Build everything
make all

# Build just the main application
make dawdiolab

# Test SACD extraction
make test-libsacd
./test_libsacd "test-isos/SACD_TEST.iso"

# Verify real files created (should be 300MB+, not 8KB!)
ls -lah test_extraction/
```

### Build Targets

- `make libaudio` - Build the unified audio library
- `make libtui` - Build the TUI framework library  
- `make dawdiolab` - Build the main application
- `make test-extraction` - Build extraction test programs
- `make clean` - Clean all build artifacts
- `make all` - Build everything

## Project Architecture

### Directory Structure

```
dawdiolab/
├── libaudio/             # Unified audio processing library
│   ├── formats/          # Format-specific handlers
│   │   ├── sacd/         # SACD ISO parsing (from libsacd)
│   │   ├── dsf/          # DSF file handling
│   │   ├── flac/         # FLAC processing
│   │   └── cue/          # CUE sheet parsing
│   ├── metadata/         # Unified metadata API
│   └── conversion/       # Format conversion engine
├── libtui/               # Custom TUI framework
│   ├── include/tui.h     # Framework API
│   └── src/              # Implementation
├── ui/                   # Application UI components
│   ├── browser/          # Enhanced file/folder browser
│   ├── metadata_editor/  # Tag editing interface
│   └── queue_manager/    # Conversion queue display
├── dawdiolab_main.c      # Application entry
├── Makefile              # Build configuration
├── test-isos/            # Test audio files
└── output/               # Conversion output directory
```

### Library Integration Strategy

1. **First choice**: Use libraries from flox catalog
2. **Second choice**: Adapt compatible open source projects
   - Clone as git submodule or vendor in `third_party/`
   - Ensure license compatibility (GPL-2.0-or-later)
3. **External tools**: Use sox-dsd for complex DSP operations

## Development Workflow

### 1. Making Changes

```bash
# Activate Flox environment
flox activate

# Edit files
vim ui/browser/browser.c

# Build and test
make dawdiolab
./dawdiolab
```

### 2. Testing Audio Processing

```bash
# Test SACD extraction
./test_extraction sacd "test-isos/SACD_TEST.iso"

# Test format conversion
./test_conversion "test-audio/track.flac" dsf

# Check output - CRITICAL: Files should be real size!
ls -lah output/
```

### 3. Adding Format Support

When adding a new audio format:

1. Check flox for existing library:
   ```bash
   flox search <format>
   flox install <library>
   ```

2. If not in flox, find open source implementation:
   ```bash
   # Add as git submodule
   git submodule add https://github.com/project/lib third_party/lib
   
   # Or vendor the code
   cp -r /path/to/lib third_party/
   ```

3. Create format handler in `libaudio/formats/<format>/`

4. Integrate with unified API

## Current State & Roadmap

### What's Working ✅

1. **SACD Extraction** - Real parsing and 378MB+ file creation
2. **Professional TUI** - Three-pane interface with navigation
3. **Track Selection** - For SACD extraction
4. **Basic Architecture** - Foundation for expansion

### In Development 🚧

1. **UI Refactoring** - Support for folder selection and multi-format
2. **Format Detection** - Automatic file type identification
3. **Metadata System** - Unified API for all formats
4. **Queue Management** - Batch processing infrastructure

### Planned Features 📋

1. **Multi-Format Support** - FLAC, WAV, DSF, DFF, CUE
2. **Batch Operations** - Process folder hierarchies
3. **Metadata Editing** - Cross-format tag editing
4. **Conversion Pipeline** - Flexible format conversion
5. **External Tool Integration** - sox-dsd for DSP

## Common Tasks

### Adding a New Dependency

```bash
# Search in flox
flox search <package>

# Install it
flox install <package>

# Or add to manifest
flox edit
```

### Integrating an Open Source Library

```bash
# Example: Adding libcue for CUE sheet support
git submodule add https://github.com/lipnitsk/libcue third_party/libcue
cd third_party/libcue
mkdir build && cd build
cmake ..
make

# Update Makefile to link against it
# Add to libaudio/formats/cue/
```

### Testing Format Detection

```bash
# Run format detection test
./test_format_detection test-audio/

# Should correctly identify:
# - SACD ISOs
# - DSF/DFF files
# - FLAC/WAV files
# - Folders with audio
# - CUE sheets
```

## UI Development Guide

### Adaptive Three-Pane System

The UI should adapt based on selection:

1. **File Browser Enhancement**
   - Show file type indicators
   - Support folder selection
   - Multi-selection with Shift/Ctrl

2. **Context-Sensitive Middle Pane**
   ```c
   switch (selected_item->type) {
       case AUDIO_TYPE_SACD_ISO:
           show_track_listing(pane);
           break;
       case AUDIO_TYPE_FOLDER:
           show_folder_contents(pane);
           break;
       case AUDIO_TYPE_AUDIO_FILE:
           show_metadata_editor(pane);
           break;
       case AUDIO_TYPE_CUE:
           show_cue_preview(pane);
           break;
   }
   ```

3. **Action Pane Updates**
   - Conversion settings per format
   - Queue management controls
   - Progress for multiple operations

## Philosophy & Development Principles

### Core Philosophy

**Real functionality over mock implementations.** Every feature must work with actual audio files and produce real results.

### Development Strategy

1. **Pragmatic Reuse** - Adapt existing solutions when available
2. **Incremental Progress** - Maintain working functionality at each step
3. **User-Centric Design** - Build what users actually need
4. **Clean Architecture** - Modular design for maintainability

### Integration Approach

- **Libraries**: Integrate directly (statically or dynamically linked)
- **Complex Tools**: Call externally (e.g., sox-dsd)
- **Hybrid Model**: DAWdioLab orchestrates, specialized tools execute

### License Compliance

- Project license: GPL-2.0-or-later (following SoX)
- Ensure all integrated libraries are compatible
- Properly attribute all adapted code
- Document modifications clearly

## Quick Command Reference

```bash
# Environment setup
flox activate              # Enter development environment
cd dawdiolab              # Navigate to project

# Build commands
make clean                # Clean build artifacts
make all                  # Build everything
make dawdiolab           # Build main application
./dawdiolab              # Run the application

# Testing commands
make test-extraction     # Build test programs
./test_extraction <type> <file>  # Test specific format
ls -lah output/          # Verify real output files

# Development helpers
gdb ./dawdiolab          # Debug with GDB
valgrind ./dawdiolab     # Memory check
bear -- make             # Generate compile_commands.json
```

## Remember

*"ffs i'm building something i'm going to use. i want something that has real functionality."*

This quote captures the project's core mission: **build real tools, not demonstrations.**

---

*Repository: https://github.com/barstoolbluz/dawdiolab*  
*License: GPL-2.0-or-later*  
*Last updated: June 2025*
