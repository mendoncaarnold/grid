// =============================================================
// demo/main.cpp — GridLayout WebGUI demo harness
// Matches reference screenshot exactly:
//   First Name, Last Name, Date of Birth (row 1)
//   Email, Phone, Department (row 2)
//   Job Title, Start Date, Employment Type (row 3)
//   Office Location, Manager ID, Notes (row 4)
// =============================================================
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include "GridLayout.h"

static const int W = 1400, H = 860;

int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n"; return 1;
    }
    SDL_Window* win = SDL_CreateWindow(
        "Grid Layout Demo - WebGUI",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { std::cerr << "Window: " << SDL_GetError(); return 1; }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { std::cerr << "Renderer: " << SDL_GetError(); return 1; }

    // ── Font search (cross-platform) ─────────────────────────
    const char* fonts[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "DejaVuSans.ttf"
    };
    std::string fp;
    for (auto& f : fonts)
        if (SDL_RWops* rw=SDL_RWFromFile(f,"r")) { SDL_RWclose(rw); fp=f; break; }
    if (fp.empty()) { std::cerr << "No font found\n"; return 1; }

    // ── TTF owned by harness, not by component ────────────────
    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init: " << TTF_GetError() << "\n"; return 1;
    }
    TTF_Font* fontSm = TTF_OpenFont(fp.c_str(), 11);
    TTF_Font* fontMd = TTF_OpenFont(fp.c_str(), 13);
    TTF_Font* fontLg = TTF_OpenFont(fp.c_str(), 15);
    if (!fontSm||!fontMd||!fontLg) {
        std::cerr << "TTF_OpenFont: " << TTF_GetError() << "\n"; return 1;
    }

    // ── GridLayout — borrows fonts ────────────────────────────
    GridLayout grid;
    if (!grid.init(ren, fontSm, fontMd, fontLg, W, H)) {
        std::cerr << "GridLayout::init() failed\n"; return 1;
    }

    // ── Props matching screenshot ─────────────────────────────
    grid.setColumns(3);
    grid.setCellPadding(10);
    grid.setAlign(CellAlign::Stretch);

    // ── Load data ─────────────────────────────────────────────
    std::string mockPath = "grid.mock.json";
    if (argc > 1) mockPath = argv[1];

    if (!grid.loadFromJSON(mockPath)) {
        // Hard-coded fallback — all values start empty, user types at runtime
        auto mk=[](const std::string& lbl, const std::string& ph,
                   FieldType tp, bool req){
            GridField f;
            f.label=lbl; f.value=""; f.placeholder=ph;
            f.type=tp; f.required=req;
            return f;
        };
        auto mks=[](const std::string& lbl, bool req,
                    std::vector<std::string> opts){
            GridField f;
            f.label=lbl; f.value=""; f.placeholder="Select...";
            f.type=FieldType::Select; f.required=req; f.options=opts;
            return f;
        };

        std::vector<GridField> demo;
        // Row 1
        demo.push_back(mk ("First Name",     "e.g. Jane",        FieldType::Text,  true));
        demo.push_back(mk ("Last Name",       "e.g. Doe",         FieldType::Text,  true));
        demo.push_back(mk ("Date of Birth",   "mm/dd/yyyy",       FieldType::Date,  true));
        // Row 2
        demo.push_back(mk ("Email",           "jane@example.com", FieldType::Text,  true));
        demo.push_back(mk ("Phone",           "+1 555-0100",      FieldType::Text,  false));
        demo.push_back(mks("Department",      true,
                           {"Engineering","Product","Design","DevOps","HR","Finance"}));
        // Row 3
        demo.push_back(mk ("Job Title",       "e.g. Senior Engineer", FieldType::Text, false));
        demo.push_back(mk ("Start Date",      "mm/dd/yyyy",       FieldType::Date,  true));
        demo.push_back(mks("Employment Type", true,
                           {"Full-time","Part-time","Contract","Intern"}));
        // Row 4
        demo.push_back(mks("Office Location", false,
                           {"HQ","Remote","Hybrid","Satellite"}));
        demo.push_back(mk ("Manager ID",      "e.g. EMP-0042",    FieldType::Text,     false));
        demo.push_back(mk ("Notes",           "Optional notes...",FieldType::Textarea,  false));

        grid.setFields(demo);
    }

    grid.layout(W, H);

    // ── Callbacks ─────────────────────────────────────────────
    grid.onFieldChanged([](int i, const std::string& v){
        std::cout << "field[" << i << "] = \"" << v << "\"\n";
    });
    grid.onFieldAdded  ([]()    { std::cout << "field added\n"; });
    grid.onFieldRemoved([](int i){ std::cout << "field " << i << " removed\n"; });

    // ── Main loop ─────────────────────────────────────────────
    SDL_StartTextInput();
    bool running = true;
    SDL_Event ev;

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type==SDL_QUIT) running=false;
            if (ev.type==SDL_KEYDOWN &&
                ev.key.keysym.sym==SDLK_ESCAPE) running=false;
            grid.handleEvent(ev);
        }
        SDL_SetRenderDrawColor(ren,18,19,26,255);
        SDL_RenderClear(ren);
        grid.render(ren);
        SDL_RenderPresent(ren);
        SDL_Delay(14);
    }

    // ── Cleanup — harness owns everything ─────────────────────
    SDL_StopTextInput();
    grid.shutdown();          // nulls borrowed pointers only
    TTF_CloseFont(fontSm);
    TTF_CloseFont(fontMd);
    TTF_CloseFont(fontLg);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}