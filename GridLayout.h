#pragma once
/**
 * @file    GridLayout.h
 * @brief   WebGUI SDL2 uniform grid layout container.
 *
 * Matches reference screenshot "Grid Layout Demo - WebGUI":
 *  - Dark background, no cell cards/borders
 *  - Small grey uppercase labels with red * for required
 *  - Medium-height dark rounded input boxes
 *  - Date fields: mm/dd/yyyy + calendar icon
 *  - Select fields: value + chevron ∨, dropdown with "Other..." option
 *  - Textarea: resize handle bottom-right
 *  - Toolbar: COLUMNS slider, PADDING slider, ALIGN buttons,
 *             + Add field, − Remove
 *  - Status bar: green dot + columns=N padding=Npx widgets=N rows=N
 *
 * No OpenGL · No Emscripten/WASM · No cross-component deps.
 * TTF_Init/TTF_OpenFont owned by caller. Component borrows fonts.
 *
 * Props
 * -----
 * | Name        | Type      | Default | Description                      |
 * |-------------|-----------|---------|----------------------------------|
 * | columns     | int       | 3       | Number of columns (1-5)          |
 * | cellPadding | int px    | 10      | Gap between cells                |
 * | align       | CellAlign | Stretch | Stretch|Left|Center|Right        |
 *
 * Events / Callbacks
 * ------------------
 * | Callback       | Signature                          | Fired when         |
 * |----------------|------------------------------------|--------------------|
 * | onFieldChanged | void(int index, const string& val) | Field value edited |
 * | onFieldAdded   | void()                             | +Add field clicked |
 * | onFieldRemoved | void(int index)                    | −Remove clicked    |
 */

#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <functional>

// =============================================================
// Enumerations
// =============================================================

/** @brief Widget type for a GridField. */
enum class FieldType {
    Text      = 0,  ///< Single-line text input
    Date      = 1,  ///< Date input with calendar icon, placeholder mm/dd/yyyy
    Select    = 2,  ///< Dropdown with options + "Other..." free-text fallback
    Textarea  = 3,  ///< Multi-line text area with resize handle
    Number    = 4   ///< Numeric text input
};

/** @brief Horizontal alignment of content inside each cell. */
enum class CellAlign {
    Stretch = 0,  ///< Widget fills full cell width (default)
    Left    = 1,  ///< Left-aligned
    Center  = 2,  ///< Centred
    Right   = 3   ///< Right-aligned
};

// =============================================================
// Data structures
// =============================================================

/**
 * @brief Describes one field/cell in the grid.
 */
struct GridField {
    std::string label;                          ///< Label text (rendered small grey uppercase)
    std::string value;                          ///< Current value string
    std::string placeholder;                    ///< Hint shown when value is empty
    FieldType   type        = FieldType::Text;  ///< Widget type
    bool        required    = false;            ///< Show red * beside label

    // Select-only
    std::vector<std::string> options;           ///< Dropdown option strings
    int                      selectedOption = -1; ///< Currently selected index (-1=none)
    bool                     dropOpen       = false; ///< Dropdown open state (internal)

    // "Other..." free-text mode (Select only)
    bool        isOtherMode = false; ///< true = user chose Other..., shows text input
    std::string otherValue;          ///< Text typed in Other mode

    // Internal
    bool focused = false; ///< Internal focus state
};

/**
 * @brief Colour theme matching the reference screenshot.
 */
struct GLTheme {
    SDL_Color bg          = {18,  19,  26,  255}; ///< Window & grid background
    SDL_Color toolbar     = {24,  25,  34,  255}; ///< Toolbar background
    SDL_Color toolbarBdr  = {45,  47,  65,  255}; ///< Toolbar bottom border
    SDL_Color inputBg     = {28,  30,  42,  255}; ///< Input field background
    SDL_Color inputBdr    = {52,  55,  78,  255}; ///< Input border idle
    SDL_Color inputBdrFoc = {99, 102, 241,  255}; ///< Input border focused (indigo)
    SDL_Color labelColor  = {140, 145, 175, 255}; ///< Label text (grey)
    SDL_Color valueColor  = {210, 215, 235, 255}; ///< Value text (light)
    SDL_Color phColor     = {75,  78, 105,  255}; ///< Placeholder text
    SDL_Color reqStar     = {210,  65,  65,  255}; ///< Required star red
    SDL_Color accentBg    = {99, 102, 241,  255};  ///< Active align button (indigo)
    SDL_Color accentText  = {255, 255, 255,  255};
    SDL_Color btnBg       = {32,  34,  48,  255};  ///< Toolbar button background
    SDL_Color btnBdr      = {60,  63,  88,  255};  ///< Toolbar button border
    SDL_Color btnText     = {185, 190, 215, 255};  ///< Toolbar button text
    SDL_Color statusBg    = {14,  15,  20,  255};  ///< Status bar background
    SDL_Color statusDot   = {72, 199,  94,  255};  ///< Green status dot
    SDL_Color statusText  = {75,  85, 115,  255};  ///< Status bar text
    SDL_Color otherColor  = {160, 145, 255, 255};  ///< "Other..." option purple tint
    SDL_Color chevronC    = {120, 125, 165, 255};  ///< Dropdown chevron colour
    SDL_Color calIconC    = {100, 105, 145, 255};  ///< Calendar icon colour
};

// =============================================================
// GridLayout
// =============================================================

/**
 * @brief Uniform grid layout — SDL2 / WebGUI.
 *
 * @note Caller owns TTF lifecycle. Component borrows font pointers only.
 */
class GridLayout {
public:
    GridLayout();
    ~GridLayout();

    // ── Lifecycle ────────────────────────────────────────────

    /**
     * @brief Attach to renderer and borrow pre-opened fonts.
     * @param renderer Active SDL_Renderer* (not owned).
     * @param fontSm   ~11 px — labels, status bar.
     * @param fontMd   ~13 px — values, toolbar text.
     * @param fontLg   ~15 px — logo.
     * @param w        Initial window width.
     * @param h        Initial window height.
     * @return true on success.
     */
    bool init(SDL_Renderer* renderer,
              TTF_Font* fontSm,
              TTF_Font* fontMd,
              TTF_Font* fontLg,
              int w = 1400, int h = 860);

    /** @brief Release internal state. Does NOT close fonts. */
    void shutdown();

    // ── Props ────────────────────────────────────────────────

    /** @brief Set number of columns. @param c Clamped [1,5]. Default 3. */
    void setColumns(int c);

    /** @brief Set gap between cells in pixels. @param px Clamped [0,40]. Default 10. */
    void setCellPadding(int px);

    /** @brief Set content alignment. */
    void setAlign(CellAlign a);

    /** @brief Override colour theme. */
    void setTheme(const GLTheme& t);

    int       columns()     const { return m_cols;  } ///< @return Column count.
    int       cellPadding() const { return m_pad;   } ///< @return Padding px.
    CellAlign align()       const { return m_align; } ///< @return Alignment.

    // ── Data ─────────────────────────────────────────────────

    /** @brief Replace all fields. */
    void setFields(const std::vector<GridField>& fields);

    /**
     * @brief Load fields from JSON (zero external deps).
     * Keys: label, value, type, placeholder, required, options[].
     * @param path Path to grid.mock.json.
     * @return true if at least one field parsed.
     */
    bool loadFromJSON(const std::string& path);

    /** @brief Append one field. */
    void addField(const GridField& f);

    /** @brief Remove field by zero-based index. */
    void removeField(int index);

    /** @brief Remove the last field. */
    void removeLastField();

    /** @return Total field count. */
    int fieldCount() const { return (int)m_fields.size(); }

    // ── Frame ────────────────────────────────────────────────

    /**
     * @brief Recompute all layout rectangles.
     * Call after init() and on window resize or prop changes.
     * @param winW Window width.
     * @param winH Window height.
     */
    void layout(int winW, int winH);

    /**
     * @brief Render the full component.
     * Call between SDL_RenderClear() and SDL_RenderPresent().
     * @param r Active SDL_Renderer*.
     */
    void render(SDL_Renderer* r);

    /**
     * @brief Forward SDL_Event to the component.
     * Handles mouse, text input, keyboard, window resize.
     * @param e SDL_Event from SDL_PollEvent().
     */
    void handleEvent(const SDL_Event& e);

    // ── Callbacks ────────────────────────────────────────────

    using ChangedCb = std::function<void(int, const std::string&)>; ///< (index, newValue)
    using VoidCb    = std::function<void()>;
    using IntCb     = std::function<void(int)>;                      ///< (fieldIndex)

    /** @brief Fired when a field value changes. */
    void onFieldChanged(ChangedCb cb) { m_changedCb = cb; }

    /** @brief Fired when +Add field is clicked. */
    void onFieldAdded(VoidCb cb) { m_addedCb = cb; }

    /** @brief Fired when −Remove is clicked. */
    void onFieldRemoved(IntCb cb) { m_removedCb = cb; }

private:
    // Drawing primitives
    void drawRect      (SDL_Renderer* r, SDL_Rect rc, SDL_Color c, bool fill = true);
    void drawRoundRect (SDL_Renderer* r, SDL_Rect rc, int rad, SDL_Color c, bool fill = true);
    void drawText      (SDL_Renderer* r, TTF_Font* f, const std::string& s,
                        int x, int y, SDL_Color c, int maxW = 0);
    int  textW         (TTF_Font* f, const std::string& s);
    void drawChevron   (SDL_Renderer* r, int cx, int cy, SDL_Color c);
    void drawCalIcon   (SDL_Renderer* r, int x, int y, SDL_Color c);

    // Sub-renderers
    void renderToolbar   (SDL_Renderer* r);
    void renderGrid      (SDL_Renderer* r);
    void renderField     (SDL_Renderer* r, int idx);
    void renderInput     (SDL_Renderer* r, GridField& f, SDL_Rect rc, bool focused);
    void renderSelect    (SDL_Renderer* r, GridField& f, SDL_Rect rc, bool focused);
    void renderTextarea  (SDL_Renderer* r, GridField& f, SDL_Rect rc, bool focused);
    void renderStatusBar (SDL_Renderer* r);
    void renderDropdowns (SDL_Renderer* r);

    // Internal toolbar widget types
    struct Slider {
        SDL_Rect track = {}, thumb = {};
        int value = 0, minV = 0, maxV = 1;
        bool dragging = false;
    };
    struct Btn {
        SDL_Rect    rc = {};
        std::string label;
        bool        active = false;
    };

    Slider           m_colSlider;
    Slider           m_padSlider;
    std::vector<Btn> m_alignBtns;
    Btn              m_addBtn;
    Btn              m_removeBtn;

    SDL_Rect m_toolbarRC = {};
    SDL_Rect m_statusRC  = {};

    std::vector<SDL_Rect> m_cellRects;
    std::vector<SDL_Rect> m_labelRects;
    std::vector<SDL_Rect> m_inputRects;

    // Props
    int       m_cols  = 3;
    int       m_pad   = 10;
    CellAlign m_align = CellAlign::Stretch;
    GLTheme   m_theme;

    std::vector<GridField> m_fields;
    std::vector<GridField> m_origFields; ///< saved for reset

    int m_focusIdx = -1;
    int m_winW     = 1400;
    int m_winH     = 860;
    int m_tbH      = 50;
    int m_sbH      = 28;
    int m_gpad     = 28;
    int m_rowGap   = 20;

    SDL_Renderer* m_renderer = nullptr;
    TTF_Font*     m_fontSm   = nullptr; ///< Borrowed — never closed here
    TTF_Font*     m_fontMd   = nullptr; ///< Borrowed — never closed here
    TTF_Font*     m_fontLg   = nullptr; ///< Borrowed — never closed here

    ChangedCb m_changedCb;
    VoidCb    m_addedCb;
    IntCb     m_removedCb;

    bool parseJSON(const std::string& json);
    static std::string jsonStr(const std::string& obj, const std::string& key);
    static std::string todayStr();
};

#endif // GRIDLAYOUT_H