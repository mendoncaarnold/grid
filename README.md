# GridLayout — WebGUI / SDL2

Uniform grid layout container matching the "Grid Layout Demo - WebGUI"
reference screenshot. Renders a 3×N form grid with red labels,
dark inputs, and a fully interactive toolbar.

```
GridLayout/
├── src/
│   ├── GridLayout.h        ← header: all props, events, Doxygen docs
│   └── GridLayout.cpp      ← implementation
├── demo/
│   └── main.cpp            ← isolated SDL2 demo harness
├── grid.mock.json          ← 12-field mock (3×4 grid)
├── demo-screenshot.png     ← reference screenshot
├── Makefile
└── README.md
```

---

## Props

| Prop        | Type      | Default  | Description                     |
|-------------|-----------|----------|---------------------------------|
| columns     | int       | 3        | Number of columns (1-5)         |
| cellPadding | int px    | 10       | Gap between columns             |
| align       | CellAlign | Stretch  | Stretch|Left|Center|Right       |
| border      | bool      | false    | Draw border around each cell    |
| bgColor     | SDL_Color | dark     | Grid canvas background colour   |

## Field types

| FieldType | Rendered as                                  |
|-----------|----------------------------------------------|
| Text      | Single-line input with placeholder           |
| Date      | Input showing today's date as placeholder    |
| Select    | Dropdown with options list                   |
| Textarea  | Multi-line input with resize handle          |
| Number    | Numeric input                                |

---

## Events / Callbacks

| Callback       | Signature                          | Fired when         |
|----------------|------------------------------------|--------------------|
| onFieldChanged | void(int index, const string& val) | Field value edited |
| onFieldAdded   | void()                             | +Add clicked       |
| onFieldRemoved | void(int index)                    | Remove clicked     |
| onValidate     | void()                             | Validate clicked   |
| onClear        | void()                             | Clear clicked      |
| onReset        | void()                             | Reset clicked      |

---

## Toolbar

| Control         | Action                                     |
|-----------------|--------------------------------------------|
| COLS slider     | Change column count 1–5 (live)             |
| PAD slider      | Change cell padding 0–40 px (live)         |
| Stretch/Left/Center/Right | Set content alignment            |
| BORDER checkbox | Toggle cell border on/off                  |
| BG checkbox     | Toggle background colour                   |
| New Field…      | Type a name for the next field to add      |
| + Add           | Add field (uses New Field name if typed)   |
| Clear           | Clear all field values                     |
| Remove          | Remove last field                          |
| Validate        | Fire onValidate callback                   |
| Reset           | Restore all fields to original values      |

---

## Build

```bash
# Ubuntu / Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev fonts-dejavu
make && ./GridLayoutDemo

# macOS
brew install sdl2 sdl2_ttf
make && ./GridLayoutDemo
```

---

## Technical requirements

- SDL2 Window — SDL_CreateWindow + SDL_CreateRenderer
- WebGUI rendering — SDL_RenderFillRect, SDL_RenderDrawLine, TTF_RenderUTF8_Blended only
- No OpenGL · No Emscripten · No WASM
- TTF_Init/TTF_OpenFont owned by harness (main.cpp)
- Component borrows font pointers, never calls TTF_Init/Quit
- JSON parsed with zero-dependency bespoke tokeniser
- No cross-component dependencies