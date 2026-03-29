// =============================================================
// demo/main.cpp — "Grid Layout Demo - WebGUI"
// Matches reference screenshot pixel-perfect
// =============================================================
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include "GridLayout.h"

static const int W=1456, H=856;

int main(int argc, char* argv[])
{
    if(SDL_Init(SDL_INIT_VIDEO)<0){std::cerr<<"SDL_Init: "<<SDL_GetError()<<"\n";return 1;}
    SDL_Window* win=SDL_CreateWindow(
        "Grid Layout Demo - WebGUI",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!win){std::cerr<<"Window: "<<SDL_GetError();return 1;}
    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){std::cerr<<"Renderer: "<<SDL_GetError();return 1;}

    // ── Font search ───────────────────────────────────────────
    const char* fonts[]={
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf","DejaVuSans.ttf"
    };
    const char* bolds[]={
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "C:/Windows/Fonts/arialbd.ttf","DejaVuSans-Bold.ttf"
    };
    std::string fp,bfp;
    for(auto& f:fonts) if(SDL_RWops* rw=SDL_RWFromFile(f,"r")){SDL_RWclose(rw);fp=f;break;}
    for(auto& f:bolds) if(SDL_RWops* rw=SDL_RWFromFile(f,"r")){SDL_RWclose(rw);bfp=f;break;}
    if(fp.empty()){std::cerr<<"No font\n";return 1;}
    if(bfp.empty()) bfp=fp;

    if(TTF_Init()<0){std::cerr<<"TTF_Init: "<<TTF_GetError()<<"\n";return 1;}
    TTF_Font* fsm=TTF_OpenFont(fp.c_str(),11);
    TTF_Font* fmd=TTF_OpenFont(fp.c_str(),13);
    TTF_Font* flg=TTF_OpenFont(fp.c_str(),15);
    TTF_Font* fbd=TTF_OpenFont(bfp.c_str(),11);
    if(!fsm||!fmd||!flg||!fbd){std::cerr<<"Font open: "<<TTF_GetError()<<"\n";return 1;}

    GridLayout grid;
    if(!grid.init(ren,fsm,fmd,flg,fbd,W,H)){std::cerr<<"init failed\n";return 1;}

    grid.setColumns(3);
    grid.setCellPadding(10);
    grid.setAlign(CellAlign::Stretch);
    grid.setBorder(false);

    std::string mock=(argc>1)?argv[1]:"grid.mock.json";
    if(!grid.loadFromJSON(mock)){
        // Hard-coded fallback — all fields empty, user types at runtime
        // Fallback fields matching screenshot — some pre-filled (white), some placeholder (grey)
        auto mk=[](const std::string& l,const std::string& val,
                   const std::string& ph,FieldType tp,bool req){
            GridField f; f.label=l; f.value=val; f.placeholder=ph;
            f.type=tp; f.required=req; return f;
        };
        auto ms=[](const std::string& l,bool req,std::vector<std::string> opts){
            GridField f; f.label=l; f.value=""; f.placeholder="Select...";
            f.type=FieldType::Select; f.required=req; f.options=opts; return f;
        };
        std::vector<GridField> d;
        d.push_back(mk("First Name",     "Jane",           "e.g. Jane",            FieldType::Text,     true));
        d.push_back(mk("Last Name",      "Doe",            "e.g. Doe",             FieldType::Text,     true));
        d.push_back(mk("Date of Birth",  "",               "18/03/2026",           FieldType::Date,     true));
        d.push_back(mk("Email",          "",               "jane@example.com",     FieldType::Text,     true));
        d.push_back(mk("Phone",          "",               "+1 555-0100",          FieldType::Text,     false));
        d.push_back(ms("Department",     true, {"Engineering","Product","Design","DevOps","HR","Finance"}));
        d.push_back(mk("Job Title",      "Senior Engineer","e.g. Senior Engineer", FieldType::Text,     false));
        d.push_back(mk("Start Date",     "",               "18/03/2026",           FieldType::Date,     true));
        d.push_back(ms("Employment Type",true, {"Full-time","Part-time","Contract","Intern"}));
        d.push_back(ms("Office Location",false,{"HQ","Remote","Hybrid","Satellite"}));
        d.push_back(mk("Manager ID",     "EMP-0042",       "e.g. EMP-0042",        FieldType::Text,     false));
        d.push_back(mk("Notes",          "",               "Optional notes...",    FieldType::Textarea, false));
        grid.setFields(d);
    }

    grid.layout(W,H);

    grid.onFieldChanged([](int i,const std::string& v){std::cout<<"field["<<i<<"]=\""<<v<<"\"\n";});
    grid.onFieldAdded  ([](){std::cout<<"field added\n";});
    grid.onFieldRemoved([](int i){std::cout<<"field "<<i<<" removed\n";});
    grid.onValidate    ([](){std::cout<<"validate\n";});
    grid.onClear       ([](){std::cout<<"clear\n";});
    grid.onReset       ([](){std::cout<<"reset\n";});

    SDL_StartTextInput();
    bool running=true; SDL_Event ev;
    while(running){
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=false;
            if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym==SDLK_ESCAPE) running=false;
            grid.handleEvent(ev);
        }
        SDL_SetRenderDrawColor(ren,14,14,20,255);
        SDL_RenderClear(ren);
        grid.render(ren);
        SDL_RenderPresent(ren);
        SDL_Delay(14);
    }

    SDL_StopTextInput();
    grid.shutdown();
    TTF_CloseFont(fsm);TTF_CloseFont(fmd);TTF_CloseFont(flg);TTF_CloseFont(fbd);
    TTF_Quit();SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();
    return 0;
}