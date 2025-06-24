# libtui - Beautiful Terminal User Interface Library

A reusable TUI library inspired by Harlequin's elegant design, featuring mouse support, themeable interfaces, and flexible pane management.

## Features

- **Harlequin-inspired aesthetics**: Clean borders, high contrast, beautiful color scheme
- **Mouse support**: Click to activate panes, scroll support
- **Themeable**: Easy to create and apply custom themes
- **Flexible layouts**: Support for 1, 2, or 3-pane layouts
- **Event-driven architecture**: Clean separation of UI and logic
- **Reusable components**: Not tied to any specific application

## Building

```bash
# In flox environment with ncurses
flox activate
cd libtui
make

# Build examples
cd examples
make
```

## Quick Start

```c
#include "tui.h"

int main(void) {
    // Create and initialize app
    tui_app_t *app = tui_create_app();
    tui_init(app);
    
    // Enable mouse support
    tui_enable_mouse(app);
    
    // Create window and panes
    tui_window_t *window = tui_create_window(app);
    app->main_window = window;
    
    tui_pane_t *pane = tui_create_pane(TUI_PANE_CUSTOM);
    tui_pane_set_title(pane, "My Pane");
    tui_window_add_pane(window, pane);
    
    // Run the app
    tui_run(app);
    
    // Cleanup
    tui_cleanup(app);
    tui_destroy_app(app);
    
    return 0;
}
```

## API Overview

### Core Functions
- `tui_create_app()` - Create application instance
- `tui_init()` - Initialize ncurses and colors
- `tui_run()` - Run the event loop
- `tui_quit()` - Exit the application
- `tui_cleanup()` - Clean up ncurses

### Themes
- `tui_theme_harlequin()` - Beautiful dark theme inspired by Harlequin
- `tui_theme_default()` - Standard theme
- `tui_set_theme()` - Apply a theme

### Mouse Support
- `tui_enable_mouse()` - Enable mouse events
- `tui_disable_mouse()` - Disable mouse events
- Automatic pane activation on click

### Pane Management
- `tui_create_pane()` - Create a new pane
- `tui_pane_set_title()` - Set pane title
- Custom draw callbacks for content
- Event handlers for keyboard and mouse

## Color Scheme

The Harlequin theme features:
- Black background (#000000)
- White text with varying intensity
- Cyan highlights for selections
- Yellow buttons/actions
- Clean gray status text

## Example Applications

See the `examples/` directory for:
- `demo.c` - Three-pane file browser demonstration

## Integration with SACD Lab

This library was extracted from the SACD Lab TUI project to be reusable across different applications while maintaining the beautiful Harlequin-inspired design.