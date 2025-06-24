# DAWdioLab Project - Comprehensive Status & Roadmap

> **Document Purpose**: This is the lodestar document for the DAWdioLab project. AI agents working on this project should continuously refer to this `PROJECT_STATUS_SUMMARY.md` for guidance on project direction, architecture, and philosophy. This document works in conjunction with `SESSION_PROGRESS_LOG.md`, which tracks incremental progress within development sessions.

> **Repository**: https://github.com/barstoolbluz/dawdiolab

## Executive Summary

DAWdioLab (formerly SACD Lab TUI) is evolving from a specialized SACD extraction tool into a comprehensive **terminal-based audio processing application**. While maintaining its core strength in SACD extraction, DAWdioLab will expand to handle multiple audio formats, batch processing, metadata editing, and flexible format conversion. The project continues its philosophy of building **real, functional tools** with a professional TUI interface.

## Project Evolution: From SACD Lab to DAWdioLab

### Original Achievement (SACD Lab)
- Built a complete, professional SACD extraction tool
- Implemented real SACD ISO parsing and audio extraction
- Created elegant ncurses TUI inspired by cmus and Harlequin
- Extracted real audio files (378MB+ DSF files)

### New Vision (DAWdioLab)
A **comprehensive audio processing TUI** that:
1. **Handles multiple formats**: SACD ISOs, DSF, DFF, FLAC, WAV, WV, PCM
2. **Processes CUE files**: Split large audio files into discrete tracks
3. **Supports batch operations**: Process entire folder hierarchies
4. **Enables metadata editing**: Edit tags for any audio format
5. **Provides flexible conversion**: Configure format/quality per file or batch

## Core Use Cases

### 1. Multi-Album Batch Processing
- User selects a parent folder containing dozens of album subfolders
- Each subfolder may contain:
  - A single large audio file + CUE file
  - Individual track files
  - An SACD ISO
- User can selectively choose which albums to process
- Edit metadata for selected albums
- Configure conversion settings per album

### 2. Format Conversion Pipeline
- Detect and queue various audio formats
- Apply consistent conversion settings across selections
- Handle format-specific requirements (DSF→PCM, SACD→FLAC, etc.)

### 3. CUE-Based Track Splitting
- Load CUE files to parse track boundaries
- Split large audio files (FLAC, WAV, ISO) into tracks
- Preserve and edit metadata during splitting

## Technical Architecture (Evolved)

```
DAWdioLab Structure:
├── libaudio/             # Unified audio processing library
│   ├── formats/          # Format-specific handlers
│   │   ├── sacd/         # SACD ISO parsing (from libsacd)
│   │   ├── dsf/          # DSF file handling
│   │   ├── dff/          # DFF file handling
│   │   ├── flac/         # FLAC processing
│   │   ├── wav/          # WAV/PCM handling
│   │   └── cue/          # CUE sheet parsing
│   ├── metadata/         # Unified metadata API
│   ├── conversion/       # Format conversion engine
│   └── queue/            # Batch processing queue
├── libtui/               # TUI framework (existing)
├── ui/                   # Application UI components
│   ├── browser/          # Enhanced file/folder browser
│   ├── metadata_editor/  # Tag editing interface
│   ├── queue_manager/    # Conversion queue display
│   └── settings/         # Conversion settings UI
└── dawdiolab_main.c      # Application entry point
```

## Development Strategy: Build vs. Adapt

### Core Approach
- **First choice**: Use libraries from flox catalog
- **Second choice**: Adapt compatible open source projects
- **Last resort**: Build from scratch only when necessary

### Proven Success: libsacd Example
We've already demonstrated this approach by:
1. Identifying `sacd_extract` as a working SACD implementation
2. Extracting and refactoring its core functionality into `libsacd`
3. Creating a clean, self-contained library with our own API
4. Maintaining compatibility while improving the interface

### What We Adapt (Libraries Only)

#### Format Libraries
- **FLAC**: libFLAC (reference implementation) or alternatives
- **DSD/DSF**: Existing DSF libraries from audio projects
- **CUE parsing**: libcue or extract from CD ripping tools
- **Metadata**: TagLib alternatives or format-specific implementations
- **DST decompression**: From existing SACD tools

#### What We DON'T Rebuild
- **SoX**: Use sox-dsd (https://github.com/barstoolbluz/sox-dsd) as external tool
- **Complex DSP**: Call external tools rather than reimplementing
- **Format converters**: Use established tools via process calls

### Library Integration Methods
1. **Git submodules**: Clone library repos as submodules
2. **Vendoring**: Copy source into `vendor/` or `third_party/` directory
3. **Static linking**: Build libraries and link statically
4. **System libraries**: Use system-installed versions when available

### Adaptation Guidelines
1. **License compatibility**: Ensure GPL/MIT/BSD compatibility
2. **Code quality**: Prefer well-maintained, clean codebases
3. **Minimal dependencies**: Extract only what we need
4. **Clean interfaces**: Wrap adapted code in our own APIs
5. **Attribution**: Properly credit original projects

## UI Evolution Plan

### Current State (3-Pane Layout)
1. **File Browser** - Navigate directories and files
2. **SACD Information** - Display disc metadata
3. **Extraction Progress** - Show extraction status

### Target State (Adaptive 3-Pane Layout)
1. **Enhanced Browser Pane**
   - Show folders with expansion
   - Display file format icons/indicators
   - Support multi-selection (files AND folders)
   - Show audio format detection results

2. **Context-Sensitive Middle Pane**
   - **For ISOs/Folders**: Track listing with selection
   - **For Audio Files**: Metadata editor
   - **For CUE Files**: Track split preview
   - **For Queue**: Batch operation overview

3. **Action/Settings Pane**
   - **Conversion Settings**: Format, quality, output options
   - **Progress Display**: Real-time conversion status
   - **Queue Management**: Reorder, remove, modify entries

### UI Interaction Flow
```
1. Browse & Select
   ├─ Navigate to parent folder
   ├─ Expand to see albums/files
   └─ Select items for processing

2. Configure & Edit
   ├─ Edit metadata (per file/album)
   ├─ Set conversion parameters
   └─ Preview changes

3. Process & Monitor
   ├─ Add to conversion queue
   ├─ Start batch processing
   └─ Monitor progress
```

## Development Roadmap

### Phase 1: UI Refactoring (Foundation)
- [ ] Refactor browser pane to support folder selection
- [ ] Implement multi-selection with visual indicators
- [ ] Create context-switching for middle pane
- [ ] Add queue management infrastructure
- [ ] Fix character display issues in TUI

### Phase 2: Format Detection & Handling
- [ ] Implement audio file detection system
- [ ] Create format handler interface
- [ ] Port SACD code to new architecture
- [ ] Research and adapt DSF/DFF libraries
- [ ] Integrate FLAC library (flox or adapted)
- [ ] Add WAV/PCM handlers
- [ ] Adapt CUE parser from existing project

### Phase 3: Metadata System
- [ ] Design unified metadata API
- [ ] Create metadata editor UI component
- [ ] Implement format-specific metadata readers/writers
- [ ] Add batch metadata editing support

### Phase 4: Conversion Engine
- [ ] Design conversion pipeline architecture
- [ ] Integrate with external tools (sox-dsd for DSP)
- [ ] Implement format conversion coordination
- [ ] Add quality/parameter configuration
- [ ] Create progress reporting system
- [ ] Handle multi-threaded conversions

### Phase 5: Queue & Batch Processing
- [ ] Implement job queue system
- [ ] Add queue persistence (save/load)
- [ ] Create queue management UI
- [ ] Add priority and dependency handling

### Phase 6: Polish & Extended Features
- [ ] Improve UI aesthetics and consistency
- [ ] Add keyboard shortcut customization
- [ ] Implement conversion presets
- [ ] Add logging and error recovery
- [ ] Create configuration file support

## Technical Considerations

### File Detection Strategy
```c
typedef enum {
    AUDIO_TYPE_SACD_ISO,
    AUDIO_TYPE_DSF,
    AUDIO_TYPE_DFF,
    AUDIO_TYPE_FLAC,
    AUDIO_TYPE_WAV,
    AUDIO_TYPE_CUE,
    AUDIO_TYPE_FOLDER,
    AUDIO_TYPE_UNKNOWN
} audio_type_t;

typedef struct {
    audio_type_t type;
    char* path;
    bool has_cue;
    bool is_album_folder;
    metadata_t* metadata;
} audio_item_t;
```

### Metadata Handling
- Unified metadata structure across all formats
- Lazy loading for performance
- In-memory editing with explicit save
- Batch operations for common fields

### Conversion Pipeline
- **Internal handling**: Format detection, metadata, file I/O
- **External tools**: Complex DSP operations via sox-dsd
- **Hybrid approach**: DAWdioLab orchestrates, specialized tools execute
- Progress callbacks from external processes
- Error handling with graceful fallbacks

## Success Metrics

### Functionality
- Handles all target audio formats correctly
- Processes large folder hierarchies efficiently
- Maintains audio quality during conversion
- Preserves and edits metadata accurately

### User Experience
- Intuitive navigation and selection
- Responsive UI during long operations
- Clear progress and status reporting
- Consistent keyboard shortcuts

### Performance
- Minimal memory usage for large collections
- Efficient multi-threaded processing
- Fast file detection and scanning
- Smooth UI updates under load

## Migration Path from SACD Lab

### What We Keep
- Core SACD extraction functionality
- TUI framework and design philosophy
- Progress reporting system
- Thread-safe architecture

### What We Refactor
- File browser → Enhanced multi-format browser
- SACD info pane → Context-sensitive display
- Extraction engine → Generalized conversion engine
- Single-format focus → Multi-format architecture

### What We Add
- Format detection system
- Metadata editing capabilities
- Queue management
- CUE file support
- Batch folder processing

## Next Immediate Steps

1. **Analyze current TUI code** to understand extension points
2. **Design data structures** for multi-format support
3. **Prototype folder selection** in file browser
4. **Create metadata API** specification
5. **Plan UI mockups** for new functionality

## Development Philosophy (Enhanced)

### Core Principles
1. **Real Over Mock**: Always implement actual functionality
2. **User-Centric**: Build what users will actually use
3. **Performance**: Efficient resource usage and threading
4. **Beauty**: Professional aesthetics matter in TUI
5. **Modularity**: Clean separation of concerns
6. **Pragmatic Reuse**: Adapt existing solutions when available

### Implementation Strategy
- **Leverage existing code**: Don't reinvent the wheel
- **Maintain license compliance**: Respect original licenses
- **Improve upon sources**: Clean up and modernize adapted code
- **Document origins**: Clear attribution and modification notes

## Summary

DAWdioLab represents the natural evolution of the SACD Lab project, expanding from a specialized tool into a comprehensive audio processing suite while maintaining the same commitment to real functionality and professional design. The project will grow incrementally, maintaining working functionality at each phase while building toward the complete vision of a powerful, efficient, and beautiful terminal-based audio processing application.
