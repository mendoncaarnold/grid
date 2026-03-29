// =============================================================
// GridLayout.cpp — pixel-perfect match to reference screenshot
// =============================================================
#include "GridLayout.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <ctime>

static void sc(SDL_Renderer* r, SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);}
static bool hit(int x,int y,SDL_Rect rc){return x>=rc.x&&x<rc.x+rc.w&&y>=rc.y&&y<rc.y+rc.h;}

GridLayout::GridLayout(){}
GridLayout::~GridLayout(){shutdown();}

std::string GridLayout::todayStr(){
    time_t t=time(nullptr); struct tm* tm=localtime(&t);
    char buf[32]; snprintf(buf,sizeof(buf),"%02d/%02d/%04d",tm->tm_mday,tm->tm_mon+1,tm->tm_year+1900);
    return buf;
}

bool GridLayout::init(SDL_Renderer* renderer,
                       TTF_Font* fontSm,TTF_Font* fontMd,
                       TTF_Font* fontLg,TTF_Font* fontBold,
                       int w,int h)
{
    if(!renderer||!fontSm||!fontMd||!fontLg||!fontBold) return false;
    m_renderer=renderer; m_fontSm=fontSm; m_fontMd=fontMd;
    m_fontLg=fontLg; m_fontBold=fontBold; m_winW=w; m_winH=h;
    return true;
}
void GridLayout::shutdown(){
    m_renderer=nullptr; m_fontSm=m_fontMd=m_fontLg=m_fontBold=nullptr;
}

void GridLayout::setColumns(int c)      {m_cols =std::clamp(c,1,5);}
void GridLayout::setCellPadding(int px) {m_pad  =std::clamp(px,0,40);}
void GridLayout::setAlign(CellAlign a)  {m_align=a;}
void GridLayout::setBorder(bool b)      {m_border=b;}
void GridLayout::setTheme(const GLTheme& t){m_theme=t;}

// ── helpers ──────────────────────────────────────────────────
static void clearField(GridField& f){
    f.value=""; f.selectedOption=-1;
    f.isOtherMode=false; f.otherValue="";
}

void GridLayout::clearAllValues(){
    for(auto& f:m_fields) clearField(f);
}
void GridLayout::resetToOriginal(){
    m_fields=m_origFields; m_focusIdx=-1;
}

void GridLayout::setFields(const std::vector<GridField>& fields){
    m_fields=fields;
    for(auto& f:m_fields){
        // Date fields: if no placeholder set, use today
        if(f.type==FieldType::Date&&f.placeholder.empty()) f.placeholder=todayStr();
        // Reset internal state only — preserve value and placeholder as-is
        f.selectedOption=-1; f.dropOpen=false;
        f.isOtherMode=false; f.otherValue=""; f.focused=false;
        // Match selectedOption to value for selects
        for(int i=0;i<(int)f.options.size();++i)
            if(f.options[i]==f.value){f.selectedOption=i;break;}
    }
    m_origFields=m_fields; m_focusIdx=-1;
}

void GridLayout::addField(const GridField& f){
    GridField nf=f; clearField(nf);
    m_fields.push_back(nf); m_origFields.push_back(nf);
}
void GridLayout::removeLastField(){
    if(!m_fields.empty()){
        m_fields.pop_back();
        if(!m_origFields.empty()) m_origFields.pop_back();
        if(m_focusIdx>=(int)m_fields.size()) m_focusIdx=-1;
    }
}

// ── JSON ─────────────────────────────────────────────────────
std::string GridLayout::jsonStr(const std::string& src,const std::string& key){
    auto p=src.find("\""+key+"\""); if(p==std::string::npos) return "";
    p=src.find(':',p); if(p==std::string::npos) return "";
    p=src.find_first_not_of(" \t\r\n",p+1); if(p==std::string::npos) return "";
    if(src[p]=='"'){auto e=src.find('"',p+1);if(e==std::string::npos)return "";return src.substr(p+1,e-p-1);}
    auto e=src.find_first_of(",}\n",p); auto rv=src.substr(p,e-p);
    rv.erase(rv.find_last_not_of(" \t\r\n")+1); return rv;
}

bool GridLayout::parseJSON(const std::string& json){
    m_fields.clear(); size_t pos=0;
    while((pos=json.find('{',pos))!=std::string::npos){
        auto end=json.find('}',pos); if(end==std::string::npos) break;
        std::string obj=json.substr(pos,end-pos+1);
        GridField f;
        f.label       = jsonStr(obj,"label");
        f.value       = jsonStr(obj,"value");       // pre-filled shown as white text
        f.placeholder = jsonStr(obj,"placeholder"); // grey hint shown when value is empty
        f.required    = (jsonStr(obj,"required")=="true");

        std::string tp=jsonStr(obj,"type");
        if     (tp=="date")     f.type=FieldType::Date;
        else if(tp=="select")   f.type=FieldType::Select;
        else if(tp=="textarea") f.type=FieldType::Textarea;
        else if(tp=="number")   f.type=FieldType::Number;
        else                    f.type=FieldType::Text;

        if(f.type==FieldType::Date&&f.placeholder.empty()) f.placeholder=todayStr();

        auto ob=obj.find("\"options\"");
        if(ob!=std::string::npos){
            auto ab=obj.find('[',ob),ae=obj.find(']',ab);
            if(ab!=std::string::npos&&ae!=std::string::npos){
                std::string arr=obj.substr(ab+1,ae-ab-1); size_t sp=0;
                while((sp=arr.find('"',sp))!=std::string::npos){
                    auto ep=arr.find('"',sp+1); if(ep==std::string::npos) break;
                    f.options.push_back(arr.substr(sp+1,ep-sp-1)); sp=ep+1;
                }
            }
        }
        if(!f.label.empty()) m_fields.push_back(f);
        pos=end+1;
    }
    m_origFields=m_fields; return !m_fields.empty();
}

bool GridLayout::loadFromJSON(const std::string& path){
    std::ifstream ifs(path); if(!ifs) return false;
    std::ostringstream ss; ss<<ifs.rdbuf(); return parseJSON(ss.str());
}

// ── layout ────────────────────────────────────────────────────
void GridLayout::layout(int winW,int winH){
    m_winW=winW; m_winH=winH;
    m_tbH=std::max(48,std::min(60,winH/15));
    m_sbH=28;
    m_toolbarRC={0,0,winW,m_tbH};

    int ty=m_tbH/2;
    int bH=std::max(24,m_tbH-22);
    const int G=10,SG=6,VG=6;
    int slW=std::max(50,std::min(90,(winW-900)/8+55));

    int tx=16;
    // logo dots + text + badge
    int ds=5,dg=3;
    tx+=2*(ds+dg)+8;
    tx+=(m_fontLg?textW(m_fontLg,"LayoutSimple"):90)+8;
    tx+=62+G*2; // WEBGUI badge

    // COLS
    tx+=(m_fontSm?textW(m_fontSm,"COLS"):30)+SG;
    m_colSlider.minV=1;m_colSlider.maxV=5;m_colSlider.value=m_cols;
    m_colSlider.track={tx,ty-2,slW,4};
    {float f=(float)(m_cols-1)/4.f; m_colSlider.thumb={tx+(int)(f*slW)-6,ty-7,14,14};}
    tx+=slW+VG;
    tx+=(m_fontMd?textW(m_fontMd,std::to_string(m_cols)):10)+G*2;

    // PAD
    tx+=(m_fontSm?textW(m_fontSm,"PAD"):24)+SG;
    m_padSlider.minV=0;m_padSlider.maxV=40;m_padSlider.value=m_pad;
    m_padSlider.track={tx,ty-2,slW,4};
    {float f=(float)m_pad/40.f; m_padSlider.thumb={tx+(int)(f*slW)-6,ty-7,14,14};}
    tx+=slW+VG;
    tx+=(m_fontMd?textW(m_fontMd,std::to_string(m_pad)+"PX"):28)+G*2;

    // ALIGN
    tx+=(m_fontSm?textW(m_fontSm,"ALIGN"):36)+SG+2;
    m_alignBtns.clear();
    const char* aL[]={"Stretch","Left","Center","Right"};
    for(int i=0;i<4;++i){
        Btn b; b.label=aL[i]; b.active=(m_align==(CellAlign)i);
        int bw=(m_fontMd?textW(m_fontMd,aL[i]):50)+18;
        b.rc={tx,ty-bH/2,bw,bH}; tx+=bw+2;
        m_alignBtns.push_back(b);
    }
    tx+=G;

    // BORDER checkbox
    m_borderBtn.label="BORDER"; m_borderBtn.active=m_border;
    m_borderBtn.rc={tx,ty-bH/2,70,bH}; tx+=80;

    // BG
    m_bgBtn.label="BG"; m_bgBtn.active=false;
    m_bgBtn.rc={tx,ty-bH/2,28,bH}; tx+=40;

    // New Field... input
    m_newFieldRC={tx,ty-bH/2,90,bH}; tx+=100;

    // right-side action buttons — measured widths
    auto bw=[&](const std::string& lbl){return (m_fontMd?textW(m_fontMd,lbl):60)+18;};
    int addW=bw("+ Add"), clearW=bw("Clear"), removeW=bw("Remove"),
        valW=bw("Validate"), resetW=bw("Reset");
    int rp=14;
    m_resetBtn.label   ="Reset";
    m_resetBtn.rc      ={winW-rp-resetW,ty-bH/2,resetW,bH};
    m_validateBtn.label="Validate";
    m_validateBtn.rc   ={winW-rp-resetW-G-valW,ty-bH/2,valW,bH};
    m_removeBtn.label  ="Remove";
    m_removeBtn.rc     ={winW-rp-resetW-G-valW-G-removeW,ty-bH/2,removeW,bH};
    m_clearBtn.label   ="Clear";
    m_clearBtn.rc      ={winW-rp-resetW-G-valW-G-removeW-G-clearW,ty-bH/2,clearW,bH};
    m_addBtn.label     ="+ Add";
    m_addBtn.rc        ={winW-rp-resetW-G-valW-G-removeW-G-clearW-G-addW,ty-bH/2,addW,bH};

    // ── grid cells ───────────────────────────────────────────
    m_cellRects.clear(); m_labelRects.clear(); m_inputRects.clear();
    int gridX=m_gpad, gridY=m_tbH+m_gpad;
    int gridW=winW-2*m_gpad;
    int colGap=m_pad;
    int cellW=(gridW-colGap*(m_cols-1))/m_cols;
    int labelH=20, inputH=50, taH=100;

    int n=(int)m_fields.size();
    for(int i=0;i<n;++i){
        int col=i%m_cols, row=i/m_cols;
        bool isTa=(m_fields[i].type==FieldType::Textarea);
        int myIH=isTa?taH:inputH;

        int yOff=gridY;
        for(int rr=0;rr<row;++rr){
            int rh=labelH+8+inputH;
            for(int cc=0;cc<m_cols;++cc){
                int fi=rr*m_cols+cc; if(fi>=n) break;
                if(m_fields[fi].type==FieldType::Textarea)
                    rh=std::max(rh,labelH+8+taH);
            }
            yOff+=rh+m_rowGap;
        }

        int x=gridX+col*(cellW+colGap);
        int iW=cellW,iX=x;
        if(m_align!=CellAlign::Stretch){
            iW=std::min(cellW,320);
            if(m_align==CellAlign::Center) iX=x+(cellW-iW)/2;
            else if(m_align==CellAlign::Right) iX=x+cellW-iW;
        }
        m_cellRects .push_back({x,yOff,cellW,labelH+8+myIH});
        m_labelRects.push_back({iX,yOff,iW,labelH});
        m_inputRects.push_back({iX,yOff+labelH+8,iW,myIH});
    }
    m_statusRC={0,winH-m_sbH,winW,m_sbH};
}

// ── draw primitives ───────────────────────────────────────────
void GridLayout::drawRect(SDL_Renderer* r,SDL_Rect rc,SDL_Color c,bool fill){
    sc(r,c); if(fill) SDL_RenderFillRect(r,&rc); else SDL_RenderDrawRect(r,&rc);
}
void GridLayout::drawRoundRect(SDL_Renderer* r,SDL_Rect rc,int rad,SDL_Color c,bool fill){
    sc(r,c);
    if(!fill){SDL_RenderDrawRect(r,&rc);return;}
    SDL_Rect h={rc.x+rad,rc.y,rc.w-2*rad,rc.h};
    SDL_Rect v={rc.x,rc.y+rad,rc.w,rc.h-2*rad};
    SDL_RenderFillRect(r,&h); SDL_RenderFillRect(r,&v);
    for(int dy=0;dy<rad;++dy){
        int dx=(int)std::sqrt((double)(rad*rad-(rad-dy)*(rad-dy)));
        SDL_RenderDrawLine(r,rc.x+rad-dx,rc.y+dy,rc.x+rad,rc.y+dy);
        SDL_RenderDrawLine(r,rc.x+rc.w-rad,rc.y+dy,rc.x+rc.w-rad+dx,rc.y+dy);
        SDL_RenderDrawLine(r,rc.x+rad-dx,rc.y+rc.h-1-dy,rc.x+rad,rc.y+rc.h-1-dy);
        SDL_RenderDrawLine(r,rc.x+rc.w-rad,rc.y+rc.h-1-dy,rc.x+rc.w-rad+dx,rc.y+rc.h-1-dy);
    }
}
void GridLayout::drawText(SDL_Renderer* r,TTF_Font* font,const std::string& text,
                           int x,int y,SDL_Color c,int maxW){
    if(!font||text.empty()) return;
    std::string t=text;
    if(maxW>0){
        while(t.size()>1){int w=0,h=0;TTF_SizeUTF8(font,t.c_str(),&w,&h);if(w<=maxW)break;t.pop_back();}
        if(t!=text) t+="…";
    }
    SDL_Surface* s=TTF_RenderUTF8_Blended(font,t.c_str(),c); if(!s) return;
    SDL_Texture* tx=SDL_CreateTextureFromSurface(r,s); SDL_FreeSurface(s); if(!tx) return;
    int tw,th; SDL_QueryTexture(tx,nullptr,nullptr,&tw,&th);
    SDL_Rect dst={x,y,tw,th}; SDL_RenderCopy(r,tx,nullptr,&dst); SDL_DestroyTexture(tx);
}
int GridLayout::textW(TTF_Font* f,const std::string& s){
    int w=0,h=0; if(f) TTF_SizeUTF8(f,s.c_str(),&w,&h); return w;
}
void GridLayout::drawChevron(SDL_Renderer* r,int cx,int cy,SDL_Color c){
    sc(r,c);
    for(int t=0;t<2;++t){
        SDL_RenderDrawLine(r,cx-6,cy-3+t,cx,cy+4+t);
        SDL_RenderDrawLine(r,cx+6,cy-3+t,cx,cy+4+t);
    }
}
void GridLayout::drawCalIcon(SDL_Renderer* r,int x,int y,SDL_Color c){
    sc(r,c);
    SDL_Rect rc={x,y,16,14};
    SDL_RenderDrawRect(r,&rc);
    SDL_Rect top={x,y,16,4}; SDL_RenderFillRect(r,&top);
}

// ── render ────────────────────────────────────────────────────
void GridLayout::render(SDL_Renderer* r){
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    drawRect(r,{0,0,m_winW,m_winH},m_theme.bg);
    renderToolbar(r);
    renderGrid(r);
    renderStatusBar(r);
    renderDropdowns(r);
    renderCalendars(r);
}

// ── toolbar ───────────────────────────────────────────────────
void GridLayout::renderToolbar(SDL_Renderer* r){
    drawRect(r,m_toolbarRC,m_theme.toolbar);
    drawRect(r,{0,m_tbH-1,m_winW,1},m_theme.toolbarBdr);

    int ty=m_tbH/2;
    SDL_Color white={255,255,255,255};
    SDL_Color muted={130,135,165,255};

    // logo dots
    int ds=5,dg=3,lx=16,ly=ty-8;
    SDL_Color dotC={160,140,255,255};
    for(int row=0;row<2;row++)
        for(int col=0;col<2;col++)
            drawRect(r,{lx+col*(ds+dg),ly+row*(ds+dg),ds,ds},dotC);
    drawText(r,m_fontLg,"LayoutSimple",lx+2*(ds+dg)+8,ly,white);
    int bx=lx+2*(ds+dg)+8+textW(m_fontLg,"LayoutSimple")+8;
    drawRoundRect(r,{bx,ty-11,62,22},4,m_theme.webguiBg);
    drawText(r,m_fontSm,"WEBGUI",bx+9,ty-7,m_theme.webguiText);

    // COLS
    int clx=m_colSlider.track.x-textW(m_fontSm,"COLS")-6;
    drawText(r,m_fontSm,"COLS",clx,ty-6,muted);
    drawRoundRect(r,m_colSlider.track,2,m_theme.btnBdr);
    int cfw=m_colSlider.thumb.x+7-m_colSlider.track.x;
    if(cfw>0) drawRoundRect(r,{m_colSlider.track.x,m_colSlider.track.y,cfw,4},2,{180,182,210,255});
    drawRoundRect(r,m_colSlider.thumb,7,white);
    drawText(r,m_fontMd,std::to_string(m_cols),m_colSlider.track.x+m_colSlider.track.w+6,ty-7,white);

    // PAD
    int plx=m_padSlider.track.x-textW(m_fontSm,"PAD")-6;
    drawText(r,m_fontSm,"PAD",plx,ty-6,muted);
    drawRoundRect(r,m_padSlider.track,2,m_theme.btnBdr);
    int pfw=m_padSlider.thumb.x+7-m_padSlider.track.x;
    if(pfw>0) drawRoundRect(r,{m_padSlider.track.x,m_padSlider.track.y,pfw,4},2,{180,182,210,255});
    drawRoundRect(r,m_padSlider.thumb,7,white);
    drawText(r,m_fontMd,std::to_string(m_pad)+"PX",m_padSlider.track.x+m_padSlider.track.w+6,ty-7,white);

    // ALIGN label
    if(!m_alignBtns.empty())
        drawText(r,m_fontSm,"ALIGN",m_alignBtns[0].rc.x-textW(m_fontSm,"ALIGN")-6,ty-6,muted);

    // align buttons
    for(auto& b:m_alignBtns){
        SDL_Color bg=b.active?m_theme.accentBg:m_theme.btnBg;
        SDL_Color bd=b.active?m_theme.accentBg:m_theme.btnBdr;
        SDL_Color tc=b.active?white:m_theme.btnText;
        drawRoundRect(r,b.rc,4,bg);
        if(!b.active) drawRoundRect(r,b.rc,4,bd,false);
        int tw2=textW(m_fontMd,b.label);
        drawText(r,m_fontMd,b.label,b.rc.x+(b.rc.w-tw2)/2,b.rc.y+(b.rc.h-13)/2,tc);
    }

    // BORDER label + checkbox
    drawText(r,m_fontSm,"BORDER",m_borderBtn.rc.x,ty-6,muted);
    SDL_Rect cbx={m_borderBtn.rc.x+textW(m_fontSm,"BORDER")+6,ty-7,14,14};
    drawRoundRect(r,cbx,3,m_theme.btnBg); drawRoundRect(r,cbx,3,m_theme.btnBdr,false);
    if(m_border){sc(r,white);SDL_RenderDrawLine(r,cbx.x+2,cbx.y+7,cbx.x+5,cbx.y+10);SDL_RenderDrawLine(r,cbx.x+5,cbx.y+10,cbx.x+12,cbx.y+3);}

    // BG label + swatch
    drawText(r,m_fontSm,"BG",m_bgBtn.rc.x,ty-6,muted);
    SDL_Rect swatch={m_bgBtn.rc.x+textW(m_fontSm,"BG")+4,ty-7,14,14};
    drawRoundRect(r,swatch,2,m_theme.bg); drawRoundRect(r,swatch,2,m_theme.btnBdr,false);

    // New Field... input
    drawRoundRect(r,m_newFieldRC,4,m_theme.inputBg);
    drawRoundRect(r,m_newFieldRC,4,m_newFieldFoc?m_theme.inputBdrFoc:m_theme.btnBdr,false);
    {std::string d=m_newFieldVal.empty()?"New Field...":m_newFieldVal;
     SDL_Color c=m_newFieldVal.empty()?m_theme.phColor:m_theme.valueColor;
     drawText(r,m_fontMd,d,m_newFieldRC.x+6,m_newFieldRC.y+(m_newFieldRC.h-13)/2,c,m_newFieldRC.w-12);}

    // action buttons
    auto drawBtn=[&](Btn& b,SDL_Color bg,SDL_Color bd,SDL_Color tc){
        drawRoundRect(r,b.rc,4,bg); drawRoundRect(r,b.rc,4,bd,false);
        int tw2=textW(m_fontMd,b.label);
        drawText(r,m_fontMd,b.label,b.rc.x+(b.rc.w-tw2)/2,b.rc.y+(b.rc.h-13)/2,tc);
    };
    drawBtn(m_addBtn,    m_theme.btnBg,m_theme.btnBdr,m_theme.btnText);
    drawBtn(m_clearBtn,  m_theme.btnBg,m_theme.btnBdr,m_theme.btnText);
    // Remove — red text, same bg
    drawRoundRect(r,m_removeBtn.rc,4,m_theme.btnBg);
    drawRoundRect(r,m_removeBtn.rc,4,m_theme.btnBdr,false);
    {int tw2=textW(m_fontMd,m_removeBtn.label);
     drawText(r,m_fontMd,m_removeBtn.label,m_removeBtn.rc.x+(m_removeBtn.rc.w-tw2)/2,
              m_removeBtn.rc.y+(m_removeBtn.rc.h-13)/2,m_theme.removeColor);}
    // Validate — blue filled
    drawRoundRect(r,m_validateBtn.rc,4,m_theme.accentBg);
    {int tw2=textW(m_fontMd,m_validateBtn.label);
     drawText(r,m_fontMd,m_validateBtn.label,m_validateBtn.rc.x+(m_validateBtn.rc.w-tw2)/2,
              m_validateBtn.rc.y+(m_validateBtn.rc.h-13)/2,white);}
    drawBtn(m_resetBtn,  m_theme.btnBg,m_theme.btnBdr,m_theme.btnText);
}

// ── grid ──────────────────────────────────────────────────────
void GridLayout::renderGrid(SDL_Renderer* r){
    drawRect(r,{0,m_tbH,m_winW,m_winH-m_tbH-m_sbH},m_theme.bg);
    int n=(int)m_fields.size();
    for(int i=0;i<n;++i) renderField(r,i);
}

void GridLayout::renderField(SDL_Renderer* r,int idx){
    GridField& f=m_fields[idx];
    SDL_Rect lrc=m_labelRects[idx], irc=m_inputRects[idx];
    bool focused=(m_focusIdx==idx);

    // border around cell
    if(m_border){
        SDL_Rect crc=m_cellRects[idx];
        drawRoundRect(r,crc,4,m_theme.inputBdr,false);
    }

    // label — red bold uppercase
    std::string up=f.label;
    for(auto& ch:up) ch=(char)toupper((unsigned char)ch);
    drawText(r,m_fontBold,up,lrc.x,lrc.y,m_theme.labelColor,lrc.w-20);
    if(f.required){
        int lw=textW(m_fontBold,up);
        drawText(r,m_fontBold," *",lrc.x+lw,lrc.y,m_theme.reqStar);
    }

    switch(f.type){
    case FieldType::Textarea: renderTextarea(r,f,irc,focused); break;
    case FieldType::Select:   renderSelect(r,f,irc,focused);   break;
    default:                  renderInput(r,f,irc,focused);    break;
    }
}

void GridLayout::renderInput(SDL_Renderer* r,GridField& f,SDL_Rect rc,bool focused){
    SDL_Color bdr=focused?m_theme.inputBdrFoc:m_theme.inputBdr;
    drawRoundRect(r,rc,6,m_theme.inputBg);
    drawRoundRect(r,rc,6,bdr,false);

    bool isDate=(f.type==FieldType::Date);
    if(isDate) drawCalIcon(r,rc.x+rc.w-24,rc.y+(rc.h-14)/2,m_theme.calIconC);

    int maxTW=rc.w-24-(isDate?22:0);
    std::string disp=f.value.empty()?f.placeholder:f.value;
    SDL_Color tc=f.value.empty()?m_theme.phColor:m_theme.valueColor;
    drawText(r,m_fontMd,disp,rc.x+14,rc.y+(rc.h-13)/2,tc,maxTW);

    if(focused&&!f.value.empty()){
        int cw=std::min(textW(m_fontMd,f.value),maxTW);
        drawRect(r,{rc.x+14+cw,rc.y+10,1,rc.h-20},m_theme.valueColor);
    }
}

void GridLayout::renderSelect(SDL_Renderer* r,GridField& f,SDL_Rect rc,bool /*focused*/){
    if(f.isOtherMode){
        drawRoundRect(r,rc,6,m_theme.inputBg);
        drawRoundRect(r,rc,6,m_theme.inputBdrFoc,false);
        SDL_Color arC={130,135,200,255};
        int ax=rc.x+rc.w-22,ay=rc.y+rc.h/2;
        sc(r,arC);
        SDL_RenderDrawLine(r,ax,ay,ax+10,ay);
        SDL_RenderDrawLine(r,ax,ay,ax+4,ay-4);
        SDL_RenderDrawLine(r,ax,ay,ax+4,ay+4);
        std::string disp=f.otherValue.empty()?"Type custom value...":f.otherValue;
        SDL_Color tc=f.otherValue.empty()?m_theme.phColor:m_theme.valueColor;
        drawText(r,m_fontMd,disp,rc.x+14,rc.y+(rc.h-13)/2,tc,rc.w-42);
        if(!f.otherValue.empty()){
            int cw=std::min(textW(m_fontMd,f.otherValue),rc.w-42);
            drawRect(r,{rc.x+14+cw,rc.y+10,1,rc.h-20},m_theme.valueColor);
        }
        return;
    }
    SDL_Color bdr=f.dropOpen?m_theme.inputBdrFoc:m_theme.inputBdr;
    drawRoundRect(r,rc,6,m_theme.inputBg);
    drawRoundRect(r,rc,6,bdr,false);
    drawChevron(r,rc.x+rc.w-18,rc.y+rc.h/2,m_theme.chevronC);
    std::string disp=f.value.empty()?f.placeholder:f.value;
    SDL_Color tc=f.value.empty()?m_theme.phColor:m_theme.valueColor;
    drawText(r,m_fontMd,disp,rc.x+14,rc.y+(rc.h-13)/2,tc,rc.w-36);
}

void GridLayout::renderTextarea(SDL_Renderer* r,GridField& f,SDL_Rect rc,bool focused){
    SDL_Color bdr=focused?m_theme.inputBdrFoc:m_theme.inputBdr;
    drawRoundRect(r,rc,6,m_theme.inputBg);
    drawRoundRect(r,rc,6,bdr,false);
    SDL_Color rhC={65,70,105,255}; sc(r,rhC);
    for(int i=1;i<=3;++i)
        SDL_RenderDrawLine(r,rc.x+rc.w-2,rc.y+rc.h-2-i*4,rc.x+rc.w-2-i*4,rc.y+rc.h-2);
    std::string disp=f.value.empty()?f.placeholder:f.value;
    SDL_Color tc=f.value.empty()?m_theme.phColor:m_theme.valueColor;
    drawText(r,m_fontMd,disp,rc.x+14,rc.y+12,tc,rc.w-28);
}

void GridLayout::renderDropdowns(SDL_Renderer* r){
    for(int i=0;i<(int)m_fields.size();++i){
        GridField& f=m_fields[i];
        if(!f.dropOpen||f.type!=FieldType::Select||f.isOtherMode) continue;
        SDL_Rect base=m_inputRects[i];
        int nO=(int)f.options.size();
        int oh=(nO+1)*34+8;
        SDL_Rect drop={base.x,base.y+base.h+2,base.w,oh};
        drawRoundRect(r,drop,6,m_theme.inputBg);
        drawRoundRect(r,drop,6,m_theme.inputBdrFoc,false);
        for(int j=0;j<nO;++j){
            SDL_Rect item={drop.x,drop.y+4+j*34,drop.w,34};
            bool sel=(j==f.selectedOption);
            if(sel) drawRoundRect(r,{item.x+4,item.y+2,item.w-8,item.h-4},4,m_theme.accentBg);
            SDL_Color tc=sel?SDL_Color{255,255,255,255}:m_theme.valueColor;
            drawText(r,m_fontMd,f.options[j],item.x+14,item.y+10,tc,item.w-24);
        }
        // Other... row
        int oy=drop.y+4+nO*34;
        drawRect(r,{drop.x+8,oy,drop.w-16,1},{55,60,95,255});
        SDL_Rect oi={drop.x,oy+1,drop.w,34};
        sc(r,m_theme.otherColor);
        int px=oi.x+14,py=oi.y+10;
        SDL_RenderDrawLine(r,px,py+7,px+8,py);
        SDL_RenderDrawLine(r,px+1,py+7,px+9,py);
        SDL_RenderDrawLine(r,px,py+7,px+2,py+9);
        drawText(r,m_fontMd,"Other...",oi.x+28,oi.y+10,m_theme.otherColor,oi.w-36);
    }
}

// ── calendar picker ───────────────────────────────────────────
static const char* kMonthNames[]={"January","February","March","April","May","June",
                                   "July","August","September","October","November","December"};
static int daysInMonth(int y,int m){
    if(m==2) return ((y%4==0&&y%100!=0)||y%400==0)?29:28;
    const int t[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    return t[m];
}
static int firstWeekday(int y,int m){
    struct tm t={}; t.tm_year=y-1900; t.tm_mon=m-1; t.tm_mday=1;
    mktime(&t); return t.tm_wday;
}

void GridLayout::renderCalendars(SDL_Renderer* r){
    if(m_calFieldIdx<0||m_calFieldIdx>=(int)m_inputRects.size()) return;
    SDL_Rect base=m_inputRects[m_calFieldIdx];
    const int CW=252,CH=232,cellSz=30,hdrH=36,dowH=22;
    int cx=base.x, cy=base.y+base.h+4;
    if(cx+CW>m_winW) cx=m_winW-CW-4;
    if(cy+CH>m_winH-m_sbH) cy=base.y-CH-4;

    SDL_Color popBg ={28, 30, 42,255};
    SDL_Color popBdr={99,102,241,255};
    SDL_Color hdrBg ={36, 38, 56,255};
    SDL_Color dayC  ={210,215,235,255};
    SDL_Color dowC  ={100,105,145,255};
    SDL_Color selBg ={99,102,241,255};
    SDL_Color navC  ={180,185,220,255};
    SDL_Color white ={255,255,255,255};
    SDL_Color shadow={0,0,0,80};

    drawRoundRect(r,{cx+3,cy+3,CW,CH},6,shadow);
    drawRoundRect(r,{cx,cy,CW,CH},6,popBg);
    drawRoundRect(r,{cx,cy,CW,CH},6,popBdr,false);

    // header
    drawRoundRect(r,{cx,cy,CW,hdrH},6,hdrBg);
    sc(r,navC);
    SDL_RenderDrawLine(r,cx+14,cy+hdrH/2,   cx+20,cy+hdrH/2-6);
    SDL_RenderDrawLine(r,cx+14,cy+hdrH/2,   cx+20,cy+hdrH/2+6);
    SDL_RenderDrawLine(r,cx+CW-14,cy+hdrH/2,cx+CW-20,cy+hdrH/2-6);
    SDL_RenderDrawLine(r,cx+CW-14,cy+hdrH/2,cx+CW-20,cy+hdrH/2+6);
    std::string hdr=std::string(kMonthNames[m_calMonth-1])+" "+std::to_string(m_calYear);
    int hw=textW(m_fontMd,hdr);
    drawText(r,m_fontMd,hdr,cx+(CW-hw)/2,cy+(hdrH-13)/2,white);

    // day-of-week
    const char* dow[]={"Su","Mo","Tu","We","Th","Fr","Sa"};
    int dowy=cy+hdrH+4;
    for(int i=0;i<7;++i)
        drawText(r,m_fontSm,dow[i],cx+6+i*cellSz+(cellSz-textW(m_fontSm,dow[i]))/2,dowy,dowC);

    // selected day
    int selDay=-1,selMon=-1,selYear=-1;
    {const std::string& v=m_fields[m_calFieldIdx].value;
     if(v.size()==10&&v[2]=='/'&&v[5]=='/')
         sscanf(v.c_str(),"%d/%d/%d",&selDay,&selMon,&selYear);}

    // day grid
    int fd=firstWeekday(m_calYear,m_calMonth);
    int nd=daysInMonth(m_calYear,m_calMonth);
    int gridy=dowy+dowH;
    for(int d=1;d<=nd;++d){
        int slot=fd+d-1, col=slot%7, row=slot/7;
        int dx=cx+6+col*cellSz, dy=gridy+row*(cellSz-2);
        bool isSel=(d==selDay&&m_calMonth==selMon&&m_calYear==selYear);
        std::string ds=std::to_string(d);
        int dtw=textW(m_fontMd,ds);
        if(isSel){
            drawRoundRect(r,{dx,dy,cellSz-2,cellSz-4},4,selBg);
            drawText(r,m_fontMd,ds,dx+(cellSz-2-dtw)/2,dy+6,white);
        } else {
            drawText(r,m_fontMd,ds,dx+(cellSz-2-dtw)/2,dy+6,dayC);
        }
    }
}

void GridLayout::renderStatusBar(SDL_Renderer* r){
    drawRect(r,m_statusRC,m_theme.statusBg);
    int n=(int)m_fields.size(), rows=(n+m_cols-1)/m_cols;
    drawRoundRect(r,{10,m_statusRC.y+(m_sbH-8)/2,8,8},4,m_theme.statusDot);
    std::string msg="columns="+std::to_string(m_cols)+" padding="+std::to_string(m_pad)
        +"px widgets="+std::to_string(n)+" rows="+std::to_string(rows);
    drawText(r,m_fontSm,msg,24,m_statusRC.y+(m_sbH-11)/2,m_theme.statusText);
}

// ── events ────────────────────────────────────────────────────
void GridLayout::handleEvent(const SDL_Event& e){
    if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){
        int mx=e.button.x,my=e.button.y;
        bool handled=false;

        // ── calendar popup interaction ────────────────────────
        if(m_calFieldIdx>=0&&m_calFieldIdx<(int)m_inputRects.size()){
            SDL_Rect base=m_inputRects[m_calFieldIdx];
            const int CW=252,CH=232,cellSz=30,hdrH=36,dowH=22;
            int cx=base.x, cy=base.y+base.h+4;
            if(cx+CW>m_winW) cx=m_winW-CW-4;
            if(cy+CH>m_winH-m_sbH) cy=base.y-CH-4;
            SDL_Rect popRC={cx,cy,CW,CH};
            if(hit(mx,my,popRC)){
                // prev arrow
                if(hit(mx,my,{cx,cy,30,hdrH})){
                    if(--m_calMonth<1){m_calMonth=12;--m_calYear;}
                    return;
                }
                // next arrow
                if(hit(mx,my,{cx+CW-30,cy,30,hdrH})){
                    if(++m_calMonth>12){m_calMonth=1;++m_calYear;}
                    return;
                }
                // day cell
                int fd=firstWeekday(m_calYear,m_calMonth);
                int nd=daysInMonth(m_calYear,m_calMonth);
                int gridy=cy+hdrH+4+dowH;
                for(int d=1;d<=nd;++d){
                    int slot=fd+d-1,col=slot%7,row=slot/7;
                    int dx=cx+6+col*cellSz,dy=gridy+row*(cellSz-2);
                    if(hit(mx,my,{dx,dy,cellSz-2,cellSz-4})){
                        char buf[32];
                        snprintf(buf,sizeof(buf),"%02d/%02d/%04d",d,m_calMonth,m_calYear);
                        m_fields[m_calFieldIdx].value=buf;
                        if(m_changedCb) m_changedCb(m_calFieldIdx,buf);
                        m_calFieldIdx=-1; // close
                        return;
                    }
                }
                return; // click inside popup but not on a day — consume
            } else {
                m_calFieldIdx=-1; // click outside — close
            }
        }

        // ── calendar icon click — open picker ─────────────────
        for(int i=0;i<(int)m_fields.size();++i){
            if(m_fields[i].type!=FieldType::Date) continue;
            SDL_Rect rc=m_inputRects[i];
            SDL_Rect iconRC={rc.x+rc.w-30,rc.y,30,rc.h};
            if(hit(mx,my,iconRC)){
                m_calFieldIdx=i;
                // seed calendar to field's current value or today
                int d,mon,yr; struct tm* tm;
                const std::string& v=m_fields[i].value;
                if(v.size()==10&&v[2]=='/'&&v[5]=='/'&&
                   sscanf(v.c_str(),"%d/%d/%d",&d,&mon,&yr)==3){
                    m_calMonth=mon; m_calYear=yr;
                } else {
                    time_t t=time(nullptr); tm=localtime(&t);
                    m_calMonth=tm->tm_mon+1; m_calYear=tm->tm_year+1900;
                }
                return;
            }
        }

        // back arrow / dropdown click
        for(int i=0;i<(int)m_fields.size();++i){
            GridField& f=m_fields[i];
            if(f.isOtherMode&&f.type==FieldType::Select){
                SDL_Rect rc=m_inputRects[i];
                if(hit(mx,my,{rc.x+rc.w-30,rc.y,30,rc.h})){
                    f.isOtherMode=false;f.otherValue="";f.value="";
                    f.selectedOption=-1;m_focusIdx=-1;handled=true;break;
                }
            }
            if(!f.dropOpen) continue;
            SDL_Rect base=m_inputRects[i];
            int nO=(int)f.options.size();
            SDL_Rect drop={base.x,base.y+base.h+2,base.w,(nO+1)*34+8};
            if(hit(mx,my,drop)){
                handled=true;
                int item=(my-drop.y-4)/34;
                if(item>=0&&item<nO){
                    f.selectedOption=item;f.value=f.options[item];
                    f.isOtherMode=false;f.otherValue="";
                    if(m_changedCb) m_changedCb(i,f.value);
                } else if(item==nO){
                    f.isOtherMode=true;f.otherValue="";f.value="";
                    f.selectedOption=-1;m_focusIdx=i;
                }
            }
            f.dropOpen=false;
        }
        if(handled) return;

        // slider thumbs
        if(hit(mx,my,m_colSlider.thumb)){m_colSlider.dragging=true;return;}
        if(hit(mx,my,m_padSlider.thumb)){m_padSlider.dragging=true;return;}

        // align
        for(int i=0;i<(int)m_alignBtns.size();++i){
            if(hit(mx,my,m_alignBtns[i].rc)){
                m_align=(CellAlign)i;
                for(auto& b:m_alignBtns) b.active=false;
                m_alignBtns[i].active=true;
                layout(m_winW,m_winH);return;
            }
        }

        // border checkbox
        {SDL_Rect cbx={m_borderBtn.rc.x+textW(m_fontSm,"BORDER")+6,m_tbH/2-7,14,14};
         if(hit(mx,my,cbx)){m_border=!m_border;return;}}

        // New Field input
        if(hit(mx,my,m_newFieldRC)){m_newFieldFoc=true;m_focusIdx=-1;return;}
        else m_newFieldFoc=false;

        // +Add
        if(hit(mx,my,m_addBtn.rc)){
            GridField nf;
            nf.label=m_newFieldVal.empty()?"Field "+std::to_string(m_fields.size()+1):m_newFieldVal;
            nf.placeholder="Enter value…";nf.type=FieldType::Text;
            m_fields.push_back(nf);m_origFields.push_back(nf);
            m_newFieldVal="";layout(m_winW,m_winH);
            if(m_addedCb) m_addedCb();return;
        }
        // Clear
        if(hit(mx,my,m_clearBtn.rc)){clearAllValues();if(m_clearCb)m_clearCb();return;}
        // Remove
        if(hit(mx,my,m_removeBtn.rc)){
            if(!m_fields.empty()){
                int idx=(int)m_fields.size()-1;removeLastField();
                layout(m_winW,m_winH);if(m_removedCb)m_removedCb(idx);
            }return;
        }
        // Validate
        if(hit(mx,my,m_validateBtn.rc)){if(m_validateCb)m_validateCb();return;}
        // Reset
        if(hit(mx,my,m_resetBtn.rc)){
            resetToOriginal();layout(m_winW,m_winH);if(m_resetCb)m_resetCb();return;
        }

        // field focus / dropdown
        m_focusIdx=-1;
        for(int i=0;i<(int)m_fields.size();++i){
            if(hit(mx,my,m_inputRects[i])){
                m_focusIdx=i;
                GridField& f=m_fields[i];
                if(f.type==FieldType::Select&&!f.isOtherMode)
                    f.dropOpen=!f.dropOpen;
                break;
            }
        }
    }

    if(e.type==SDL_MOUSEBUTTONUP)
        m_colSlider.dragging=m_padSlider.dragging=false;

    if(e.type==SDL_MOUSEMOTION){
        int mx=e.motion.x;
        auto drag=[&](Slider& sl,int& prop,int mn,int mx2){
            if(!sl.dragging) return;
            float f=std::clamp((float)(mx-sl.track.x)/sl.track.w,0.f,1.f);
            prop=mn+(int)std::round(f*(mx2-mn));sl.value=prop;
            sl.thumb.x=sl.track.x+(int)(f*sl.track.w)-sl.thumb.w/2;
            layout(m_winW,m_winH);
        };
        drag(m_colSlider,m_cols,1,5);
        drag(m_padSlider,m_pad, 0,40);
    }

    if(e.type==SDL_TEXTINPUT){
        if(m_newFieldFoc){m_newFieldVal+=e.text.text;}
        else if(m_focusIdx>=0){
            auto& f=m_fields[m_focusIdx];
            if(f.type==FieldType::Select&&f.isOtherMode){
                f.otherValue+=e.text.text;f.value=f.otherValue;
                if(m_changedCb) m_changedCb(m_focusIdx,f.value);
            } else if(f.type!=FieldType::Select){
                f.value+=e.text.text;
                if(m_changedCb) m_changedCb(m_focusIdx,f.value);
            }
        }
    }

    if(e.type==SDL_KEYDOWN){
        if(m_newFieldFoc){
            if(e.key.keysym.sym==SDLK_BACKSPACE&&!m_newFieldVal.empty()) m_newFieldVal.pop_back();
            if(e.key.keysym.sym==SDLK_RETURN||e.key.keysym.sym==SDLK_ESCAPE) m_newFieldFoc=false;
        } else if(m_focusIdx>=0){
            auto& f=m_fields[m_focusIdx];
            if(f.type==FieldType::Select&&f.isOtherMode){
                if(e.key.keysym.sym==SDLK_BACKSPACE&&!f.otherValue.empty()){
                    f.otherValue.pop_back();f.value=f.otherValue;
                    if(m_changedCb) m_changedCb(m_focusIdx,f.value);
                }
                if(e.key.keysym.sym==SDLK_ESCAPE){
                    f.isOtherMode=false;f.otherValue="";f.value="";
                    f.selectedOption=-1;m_focusIdx=-1;
                }
                if(e.key.keysym.sym==SDLK_RETURN) m_focusIdx=-1;
            } else {
                if(e.key.keysym.sym==SDLK_BACKSPACE&&!f.value.empty()){
                    f.value.pop_back();if(m_changedCb)m_changedCb(m_focusIdx,f.value);
                }
                if(e.key.keysym.sym==SDLK_TAB)
                    m_focusIdx=(m_focusIdx+1)%(int)m_fields.size();
                if(e.key.keysym.sym==SDLK_RETURN) m_focusIdx=-1;
            }
        }
    }

    if(e.type==SDL_WINDOWEVENT&&
      (e.window.event==SDL_WINDOWEVENT_RESIZED||
       e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED))
        layout(e.window.data1,e.window.data2);
}