// =============================================================
// GridLayout.cpp — WebGUI SDL2 grid layout
// Matches reference screenshot exactly
// =============================================================
#include "GridLayout.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <ctime>

static void sc(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
static bool hit(int x, int y, SDL_Rect rc) {
    return x>=rc.x && x<rc.x+rc.w && y>=rc.y && y<rc.y+rc.h;
}

// ── ctor / dtor ───────────────────────────────────────────────
GridLayout::GridLayout()  {}
GridLayout::~GridLayout() { shutdown(); }

std::string GridLayout::todayStr() {
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             tm->tm_mon+1, tm->tm_mday, tm->tm_year+1900);
    return buf;
}

// ── init / shutdown ───────────────────────────────────────────
bool GridLayout::init(SDL_Renderer* renderer,
                       TTF_Font* fontSm, TTF_Font* fontMd, TTF_Font* fontLg,
                       int w, int h)
{
    if (!renderer||!fontSm||!fontMd||!fontLg) return false;
    m_renderer=renderer;
    m_fontSm=fontSm; m_fontMd=fontMd; m_fontLg=fontLg;
    m_winW=w; m_winH=h;
    return true;
}
void GridLayout::shutdown() {
    m_renderer=nullptr;
    m_fontSm=m_fontMd=m_fontLg=nullptr;
}

// ── props ─────────────────────────────────────────────────────
void GridLayout::setColumns(int c)      { m_cols =std::clamp(c,1,5); }
void GridLayout::setCellPadding(int px) { m_pad  =std::clamp(px,0,40); }
void GridLayout::setAlign(CellAlign a)  { m_align=a; }
void GridLayout::setTheme(const GLTheme& t){ m_theme=t; }

// ── data ─────────────────────────────────────────────────────
void GridLayout::setFields(const std::vector<GridField>& f) {
    m_fields = f;
    // Ensure all fields start empty — user types at runtime.
    // Move any pre-filled value to placeholder if placeholder is blank.
    for (auto& field : m_fields) {
        if (field.placeholder.empty() && !field.value.empty())
            field.placeholder = field.value;
        field.value = "";
        field.selectedOption = -1;
        field.isOtherMode = false;
        field.otherValue  = "";
    }
    m_origFields = m_fields;
    m_focusIdx = -1;
}
void GridLayout::addField(const GridField& f) {
    m_fields.push_back(f); m_origFields.push_back(f);
}
void GridLayout::removeField(int idx) {
    if (idx<0||idx>=(int)m_fields.size()) return;
    m_fields.erase(m_fields.begin()+idx);
    if (idx<(int)m_origFields.size())
        m_origFields.erase(m_origFields.begin()+idx);
    if (m_focusIdx>=(int)m_fields.size()) m_focusIdx=-1;
}
void GridLayout::removeLastField() {
    if (!m_fields.empty()) removeField((int)m_fields.size()-1);
}

// ── JSON ─────────────────────────────────────────────────────
std::string GridLayout::jsonStr(const std::string& src,
                                 const std::string& key)
{
    auto p=src.find("\""+key+"\"");
    if (p==std::string::npos) return "";
    p=src.find(':',p); if (p==std::string::npos) return "";
    p=src.find_first_not_of(" \t\r\n",p+1);
    if (p==std::string::npos) return "";
    if (src[p]=='"') {
        auto e=src.find('"',p+1);
        if (e==std::string::npos) return "";
        return src.substr(p+1,e-p-1);
    }
    auto e=src.find_first_of(",}\n",p);
    auto rv=src.substr(p,e-p);
    rv.erase(rv.find_last_not_of(" \t\r\n")+1);
    return rv;
}

bool GridLayout::parseJSON(const std::string& json) {
    m_fields.clear();
    size_t pos=0;
    while ((pos=json.find('{',pos))!=std::string::npos) {
        auto end=json.find('}',pos);
        if (end==std::string::npos) break;
        std::string obj=json.substr(pos,end-pos+1);
        GridField f;
        f.label       = jsonStr(obj,"label");
        f.placeholder = jsonStr(obj,"placeholder");
        f.required    = (jsonStr(obj,"required")=="true");

        // "value" from JSON becomes the placeholder hint so fields start empty.
        // If no explicit placeholder was set, use the JSON value as the hint.
        std::string jsonValue = jsonStr(obj,"value");
        if (f.placeholder.empty() && !jsonValue.empty())
            f.placeholder = jsonValue;
        // value always starts empty — user types it at runtime
        f.value = "";
        std::string tp= jsonStr(obj,"type");
        if      (tp=="date")     f.type=FieldType::Date;
        else if (tp=="select")   f.type=FieldType::Select;
        else if (tp=="textarea") f.type=FieldType::Textarea;
        else if (tp=="number")   f.type=FieldType::Number;
        else                     f.type=FieldType::Text;

        // parse options array
        auto ob=obj.find("\"options\"");
        if (ob!=std::string::npos) {
            auto ab=obj.find('[',ob), ae=obj.find(']',ab);
            if (ab!=std::string::npos&&ae!=std::string::npos) {
                std::string arr=obj.substr(ab+1,ae-ab-1);
                size_t sp=0;
                while ((sp=arr.find('"',sp))!=std::string::npos) {
                    auto ep=arr.find('"',sp+1);
                    if (ep==std::string::npos) break;
                    f.options.push_back(arr.substr(sp+1,ep-sp-1));
                    sp=ep+1;
                }
            }
        }
        for (int i=0;i<(int)f.options.size();++i)
            if (f.options[i]==f.value){f.selectedOption=i;break;}

        if (f.type==FieldType::Date && f.placeholder.empty())
            f.placeholder="mm/dd/yyyy";

        if (!f.label.empty()) m_fields.push_back(f);
        pos=end+1;
    }
    m_origFields=m_fields;
    return !m_fields.empty();
}

bool GridLayout::loadFromJSON(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    std::ostringstream ss; ss<<ifs.rdbuf();
    return parseJSON(ss.str());
}

// ── layout ────────────────────────────────────────────────────
void GridLayout::layout(int winW, int winH) {
    m_winW=winW; m_winH=winH;

    // Scale toolbar height: taller on smaller screens so labels fit
    m_tbH = std::max(44, std::min(56, winH / 16));
    m_sbH = 28;

    m_toolbarRC={0,0,winW,m_tbH};
    int ty = m_tbH/2;
    int bH = std::max(22, m_tbH - 20); // button height scales with toolbar

    // ── Toolbar: left-to-right flow using measured widths ────
    // All positions computed from actual font metrics so nothing
    // ever overlaps regardless of window width.

    const int GAP  = 10;  // gap between toolbar groups
    const int SGAP =  6;  // gap between label and its slider
    const int VGAP =  6;  // gap between slider and its value label

    // Slider track width scales with window: wider on larger screens
    int sliderW = std::max(55, std::min(100, (winW - 800) / 8 + 60));

    int tx = 16; // start from left edge

    // ── Logo: 2×2 dots ──────────────────────────────────────
    int ds=5, dg=3;
    int logoDotsW = 2*(ds+dg); // 16px
    tx += logoDotsW + 8;       // skip dots + gap

    // "LayoutSimple" — measure with fontLg (~15px)
    int logoTextW = m_fontLg ? textW(m_fontLg,"LayoutSimple") : 90;
    tx += logoTextW + 8;

    // WEBGUI badge: 62px wide
    int badgeW = 62;
    tx += badgeW + GAP*2;

    // ── COLUMNS label + slider ───────────────────────────────
    int colLabelW = m_fontSm ? textW(m_fontSm,"COLUMNS") : 52;
    tx += colLabelW + SGAP;

    m_colSlider.minV=1; m_colSlider.maxV=5; m_colSlider.value=m_cols;
    m_colSlider.track={tx, ty-2, sliderW, 4};
    { float f=(float)(m_cols-1)/4.f;
      m_colSlider.thumb={tx+(int)(f*sliderW)-6, ty-7, 14,14}; }
    tx += sliderW + VGAP;

    // column count value "3"
    int colValW = m_fontMd ? textW(m_fontMd, std::to_string(m_cols)) : 10;
    tx += colValW + GAP*2;

    // ── PADDING label + slider ───────────────────────────────
    int padLabelW = m_fontSm ? textW(m_fontSm,"PADDING") : 52;
    tx += padLabelW + SGAP;

    m_padSlider.minV=0; m_padSlider.maxV=40; m_padSlider.value=m_pad;
    m_padSlider.track={tx, ty-2, sliderW, 4};
    { float f=(float)m_pad/40.f;
      m_padSlider.thumb={tx+(int)(f*sliderW)-6, ty-7, 14,14}; }
    tx += sliderW + VGAP;

    // padding value "10px"
    std::string padValStr = std::to_string(m_pad)+"px";
    int padValW = m_fontMd ? textW(m_fontMd, padValStr) : 28;
    tx += padValW + GAP*2;

    // ── ALIGN label + buttons ────────────────────────────────
    int alignLabelW = m_fontSm ? textW(m_fontSm,"ALIGN") : 36;
    tx += alignLabelW + SGAP + 2;

    m_alignBtns.clear();
    const char* aL[]={"Stretch","Left","Center","Right"};
    for (int i=0;i<4;++i) {
        Btn b; b.label=aL[i]; b.active=(m_align==(CellAlign)i);
        // button width = text width + horizontal padding
        int bw = (m_fontMd ? textW(m_fontMd,aL[i]) : 50) + 16;
        b.rc={tx, ty-bH/2, bw, bH};
        tx += bw + 2;
        m_alignBtns.push_back(b);
    }

    // ── Right-side buttons: + Add field  − Remove ────────────
    // These are pinned to the right edge
    m_addBtn.label = "+ Add field";
    m_removeBtn.label = "\xe2\x88\x92 Remove";

    int addW    = (m_fontMd ? textW(m_fontMd, m_addBtn.label)    : 72) + 20;
    int removeW = (m_fontMd ? textW(m_fontMd, m_removeBtn.label) : 66) + 20;
    int rightPad = 16;

    m_removeBtn.rc = {winW - rightPad - removeW,       ty-bH/2, removeW, bH};
    m_addBtn.rc    = {winW - rightPad - removeW - GAP - addW, ty-bH/2, addW,    bH};

    // ── grid cells ───────────────────────────────────────────
    m_cellRects.clear(); m_labelRects.clear(); m_inputRects.clear();

    int gridX  = m_gpad;
    int gridY  = m_tbH + m_gpad;
    int gridW  = winW - 2*m_gpad;
    int colGap = m_pad;
    int cellW  = (gridW - colGap*(m_cols-1)) / m_cols;

    int labelH = 18;   // label row height
    int inputH = 40;   // standard input height (matches screenshot)
    int taH    = 96;   // textarea height

    int n=(int)m_fields.size();
    for (int i=0;i<n;++i) {
        int col=i%m_cols, row=i/m_cols;
        bool isTa=(m_fields[i].type==FieldType::Textarea);
        int myIH = isTa ? taH : inputH;
        int myCH = labelH+6+myIH;

        // accumulate y for rows above
        int yOff=gridY;
        for (int rr=0;rr<row;++rr) {
            int rh=labelH+6+inputH;
            for (int cc=0;cc<m_cols;++cc) {
                int fi=rr*m_cols+cc; if (fi>=n) break;
                if (m_fields[fi].type==FieldType::Textarea)
                    rh=std::max(rh,labelH+6+taH);
            }
            yOff+=rh+m_rowGap;
        }

        int x=gridX+col*(cellW+colGap);
        int iW=cellW, iX=x;
        if (m_align!=CellAlign::Stretch) {
            iW=std::min(cellW,320);
            if (m_align==CellAlign::Center) iX=x+(cellW-iW)/2;
            else if (m_align==CellAlign::Right)  iX=x+cellW-iW;
        }

        m_cellRects .push_back({x,      yOff,          cellW, myCH});
        m_labelRects.push_back({iX,     yOff,          iW,    labelH});
        m_inputRects.push_back({iX,     yOff+labelH+6, iW,    myIH});
    }

    m_statusRC={0,winH-m_sbH,winW,m_sbH};
}

// ── draw primitives ───────────────────────────────────────────
void GridLayout::drawRect(SDL_Renderer* r, SDL_Rect rc,
                           SDL_Color c, bool fill) {
    sc(r,c);
    if (fill) SDL_RenderFillRect(r,&rc);
    else       SDL_RenderDrawRect(r,&rc);
}

void GridLayout::drawRoundRect(SDL_Renderer* r, SDL_Rect rc,
                                int rad, SDL_Color c, bool fill) {
    sc(r,c);
    if (!fill) { SDL_RenderDrawRect(r,&rc); return; }
    SDL_Rect h={rc.x+rad,rc.y,rc.w-2*rad,rc.h};
    SDL_Rect v={rc.x,rc.y+rad,rc.w,rc.h-2*rad};
    SDL_RenderFillRect(r,&h);
    SDL_RenderFillRect(r,&v);
    for (int dy=0;dy<rad;++dy) {
        int dx=(int)std::sqrt((double)(rad*rad-(rad-dy)*(rad-dy)));
        SDL_RenderDrawLine(r,rc.x+rad-dx,rc.y+dy,rc.x+rad,rc.y+dy);
        SDL_RenderDrawLine(r,rc.x+rc.w-rad,rc.y+dy,rc.x+rc.w-rad+dx,rc.y+dy);
        SDL_RenderDrawLine(r,rc.x+rad-dx,rc.y+rc.h-1-dy,rc.x+rad,rc.y+rc.h-1-dy);
        SDL_RenderDrawLine(r,rc.x+rc.w-rad,rc.y+rc.h-1-dy,rc.x+rc.w-rad+dx,rc.y+rc.h-1-dy);
    }
}

void GridLayout::drawText(SDL_Renderer* r, TTF_Font* font,
                           const std::string& text,
                           int x, int y, SDL_Color c, int maxW)
{
    if (!font||text.empty()) return;
    std::string t=text;
    if (maxW>0) {
        while (t.size()>1) {
            int w=0,h=0; TTF_SizeUTF8(font,t.c_str(),&w,&h);
            if (w<=maxW) break;
            t.pop_back();
        }
        if (t!=text) t+="…";
    }
    SDL_Surface* s=TTF_RenderUTF8_Blended(font,t.c_str(),c);
    if (!s) return;
    SDL_Texture* tx=SDL_CreateTextureFromSurface(r,s);
    SDL_FreeSurface(s); if (!tx) return;
    int tw,th; SDL_QueryTexture(tx,nullptr,nullptr,&tw,&th);
    SDL_Rect dst={x,y,tw,th};
    SDL_RenderCopy(r,tx,nullptr,&dst);
    SDL_DestroyTexture(tx);
}

int GridLayout::textW(TTF_Font* f, const std::string& s) {
    int w=0,h=0; if(f) TTF_SizeUTF8(f,s.c_str(),&w,&h); return w;
}

void GridLayout::drawChevron(SDL_Renderer* r, int cx, int cy, SDL_Color c) {
    sc(r,c);
    // ∨  chevron pointing down
    for (int t=0;t<2;++t) {
        SDL_RenderDrawLine(r,cx-6,cy-3+t,cx,cy+4+t);
        SDL_RenderDrawLine(r,cx+6,cy-3+t,cx,cy+4+t);
    }
}

void GridLayout::drawCalIcon(SDL_Renderer* r, int x, int y, SDL_Color c) {
    // simple calendar icon: rect + header bar
    sc(r,c);
    SDL_RenderDrawRect(r, new SDL_Rect{x,y,16,14});
    SDL_RenderFillRect(r, new SDL_Rect{x,y,16,4});
}

// ── render ────────────────────────────────────────────────────
void GridLayout::render(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    drawRect(r,{0,0,m_winW,m_winH},m_theme.bg);
    renderToolbar(r);
    renderGrid(r);
    renderStatusBar(r);
    renderDropdowns(r);
}

// ── toolbar ───────────────────────────────────────────────────
void GridLayout::renderToolbar(SDL_Renderer* r) {
    drawRect(r,m_toolbarRC,m_theme.toolbar);
    drawRect(r,{0,m_tbH-1,m_winW,1},m_theme.toolbarBdr);

    int ty=m_tbH/2;
    SDL_Color white={255,255,255,255};
    SDL_Color muted=m_theme.labelColor;

    // ── Logo: 2×2 dot grid + "LayoutSimple" ──────────────────
    int lx=16, ly=ty-8;
    SDL_Color dotC={160,140,255,255};
    int ds=5, dg=3;
    for (int row=0;row<2;row++)
        for (int col=0;col<2;col++)
            drawRect(r,{lx+col*(ds+dg), ly+row*(ds+dg), ds,ds}, dotC);

    drawText(r,m_fontLg,"LayoutSimple",lx+2*(ds+dg)+8,ly,white);
    int badgeX=lx+2*(ds+dg)+8+textW(m_fontLg,"LayoutSimple")+8;
    drawRoundRect(r,{badgeX,ty-11,62,22},4,m_theme.accentBg);
    drawText(r,m_fontSm,"WEBGUI",badgeX+9,ty-7,white);

    // ── COLUMNS label + slider ────────────────────────────────
    // Label sits immediately left of slider track (computed in layout)
    int colLabelX = m_colSlider.track.x - textW(m_fontSm,"COLUMNS") - 6;
    drawText(r,m_fontSm,"COLUMNS", colLabelX, ty-6, muted);
    // track
    drawRoundRect(r,m_colSlider.track,2,m_theme.btnBdr);
    int cfw=m_colSlider.thumb.x+7-m_colSlider.track.x;
    if (cfw>0) drawRoundRect(r,{m_colSlider.track.x,m_colSlider.track.y,cfw,4},2,m_theme.accentBg);
    drawRoundRect(r,m_colSlider.thumb,7,m_theme.accentBg);
    // value label after track
    drawText(r,m_fontMd,std::to_string(m_cols),
             m_colSlider.track.x+m_colSlider.track.w+6, ty-7, white);

    // ── PADDING label + slider ────────────────────────────────
    int padLabelX = m_padSlider.track.x - textW(m_fontSm,"PADDING") - 6;
    drawText(r,m_fontSm,"PADDING", padLabelX, ty-6, muted);
    drawRoundRect(r,m_padSlider.track,2,m_theme.btnBdr);
    int pfw=m_padSlider.thumb.x+7-m_padSlider.track.x;
    if (pfw>0) drawRoundRect(r,{m_padSlider.track.x,m_padSlider.track.y,pfw,4},2,m_theme.accentBg);
    drawRoundRect(r,m_padSlider.thumb,7,m_theme.accentBg);
    drawText(r,m_fontMd,std::to_string(m_pad)+"px",
             m_padSlider.track.x+m_padSlider.track.w+6, ty-7, white);

    // ── ALIGN label ───────────────────────────────────────────
    if (!m_alignBtns.empty()) {
        int alignLabelX = m_alignBtns[0].rc.x - textW(m_fontSm,"ALIGN") - 6;
        drawText(r,m_fontSm,"ALIGN", alignLabelX, ty-6, muted);
    }

    // ── Align buttons ─────────────────────────────────────────
    for (auto& b:m_alignBtns) {
        SDL_Color bg  = b.active ? m_theme.accentBg : m_theme.btnBg;
        SDL_Color bdr = b.active ? m_theme.accentBg : m_theme.btnBdr;
        SDL_Color tc  = b.active ? m_theme.accentText : m_theme.btnText;
        drawRoundRect(r,b.rc,4,bg);
        if (!b.active) drawRoundRect(r,b.rc,4,bdr,false);
        int tw2=textW(m_fontMd,b.label);
        drawText(r,m_fontMd,b.label,
                 b.rc.x+(b.rc.w-tw2)/2,
                 b.rc.y+(b.rc.h-13)/2, tc);
    }

    // ── + Add field ───────────────────────────────────────────
    drawRoundRect(r,m_addBtn.rc,5,m_theme.btnBg);
    drawRoundRect(r,m_addBtn.rc,5,m_theme.btnBdr,false);
    { int tw2=textW(m_fontMd,m_addBtn.label);
      drawText(r,m_fontMd,m_addBtn.label,
               m_addBtn.rc.x+(m_addBtn.rc.w-tw2)/2,
               m_addBtn.rc.y+(m_addBtn.rc.h-13)/2,
               m_theme.btnText); }

    // ── − Remove ─────────────────────────────────────────────
    drawRoundRect(r,m_removeBtn.rc,5,m_theme.btnBg);
    drawRoundRect(r,m_removeBtn.rc,5,m_theme.btnBdr,false);
    { int tw2=textW(m_fontMd,m_removeBtn.label);
      drawText(r,m_fontMd,m_removeBtn.label,
               m_removeBtn.rc.x+(m_removeBtn.rc.w-tw2)/2,
               m_removeBtn.rc.y+(m_removeBtn.rc.h-13)/2,
               m_theme.btnText); }
}

// ── grid ──────────────────────────────────────────────────────
void GridLayout::renderGrid(SDL_Renderer* r) {
    drawRect(r,{0,m_tbH,m_winW,m_winH-m_tbH-m_sbH},m_theme.bg);
    int n=(int)m_fields.size();
    for (int i=0;i<n;++i) renderField(r,i);
}

void GridLayout::renderField(SDL_Renderer* r, int idx) {
    GridField& f  = m_fields[idx];
    SDL_Rect   lrc= m_labelRects[idx];
    SDL_Rect   irc= m_inputRects[idx];
    bool focused  = (m_focusIdx==idx);

    // ── Label: small grey uppercase + red * ──────────────────
    std::string upLabel=f.label;
    for (auto& ch:upLabel) ch=(char)toupper((unsigned char)ch);
    drawText(r,m_fontSm,upLabel,lrc.x,lrc.y,m_theme.labelColor,lrc.w-20);
    if (f.required) {
        int lw=textW(m_fontSm,upLabel);
        drawText(r,m_fontSm," *",lrc.x+lw,lrc.y,m_theme.reqStar);
    }

    // ── Widget ────────────────────────────────────────────────
    switch(f.type) {
    case FieldType::Textarea: renderTextarea(r,f,irc,focused); break;
    case FieldType::Select:   renderSelect(r,f,irc,focused);   break;
    default:                  renderInput(r,f,irc,focused);    break;
    }
}

void GridLayout::renderInput(SDL_Renderer* r, GridField& f,
                              SDL_Rect rc, bool focused)
{
    SDL_Color bdr=focused?m_theme.inputBdrFoc:m_theme.inputBdr;
    drawRoundRect(r,rc,5,m_theme.inputBg);
    drawRoundRect(r,rc,5,bdr,false);

    bool isDate=(f.type==FieldType::Date);
    if (isDate) {
        // calendar icon on right
        drawCalIcon(r, rc.x+rc.w-24, rc.y+(rc.h-14)/2, m_theme.calIconC);
    }

    std::string disp=f.value.empty()?f.placeholder:f.value;
    SDL_Color tc=f.value.empty()?m_theme.phColor:m_theme.valueColor;
    int maxTW=rc.w-24-(isDate?24:0);
    drawText(r,m_fontMd,disp,rc.x+12,rc.y+(rc.h-13)/2,tc,maxTW);

    if (focused && !f.value.empty()) {
        int cw=std::min(textW(m_fontMd,f.value),maxTW);
        drawRect(r,{rc.x+12+cw,rc.y+8,1,rc.h-16},m_theme.valueColor);
    }
}

void GridLayout::renderSelect(SDL_Renderer* r, GridField& f,
                               SDL_Rect rc, bool /*focused*/)
{
    // ── Other mode: text input with ← back arrow ─────────────
    if (f.isOtherMode) {
        drawRoundRect(r,rc,5,m_theme.inputBg);
        drawRoundRect(r,rc,5,m_theme.inputBdrFoc,false); // always focused look

        // ← back arrow (return to dropdown)
        SDL_Color arC={140,145,200,255};
        int ax=rc.x+rc.w-22, ay=rc.y+rc.h/2;
        sc(r,arC);
        SDL_RenderDrawLine(r,ax,ay,ax+10,ay);
        SDL_RenderDrawLine(r,ax,ay,ax+4,ay-4);
        SDL_RenderDrawLine(r,ax,ay,ax+4,ay+4);

        std::string disp=f.otherValue.empty()?"Type custom value...":f.otherValue;
        SDL_Color tc=f.otherValue.empty()?m_theme.phColor:m_theme.valueColor;
        drawText(r,m_fontMd,disp,rc.x+12,rc.y+(rc.h-13)/2,tc,rc.w-42);

        if (!f.otherValue.empty()) {
            int cw=std::min(textW(m_fontMd,f.otherValue),rc.w-42);
            drawRect(r,{rc.x+12+cw,rc.y+8,1,rc.h-16},m_theme.valueColor);
        }
        return;
    }

    // ── Normal dropdown display ───────────────────────────────
    SDL_Color bdr=f.dropOpen?m_theme.inputBdrFoc:m_theme.inputBdr;
    drawRoundRect(r,rc,5,m_theme.inputBg);
    drawRoundRect(r,rc,5,bdr,false);
    drawChevron(r,rc.x+rc.w-18,rc.y+rc.h/2,m_theme.chevronC);

    std::string disp=f.value.empty()?f.placeholder:f.value;
    SDL_Color tc=f.value.empty()?m_theme.phColor:m_theme.valueColor;
    drawText(r,m_fontMd,disp,rc.x+12,rc.y+(rc.h-13)/2,tc,rc.w-36);
}

void GridLayout::renderTextarea(SDL_Renderer* r, GridField& f,
                                 SDL_Rect rc, bool focused)
{
    SDL_Color bdr=focused?m_theme.inputBdrFoc:m_theme.inputBdr;
    drawRoundRect(r,rc,5,m_theme.inputBg);
    drawRoundRect(r,rc,5,bdr,false);

    // resize handle (bottom-right diagonal lines)
    SDL_Color rhCol={70,75,110,255};
    sc(r, rhCol);
    for (int i=1;i<=3;++i)
        SDL_RenderDrawLine(r,
            rc.x+rc.w-2, rc.y+rc.h-2-i*4,
            rc.x+rc.w-2-i*4, rc.y+rc.h-2);

    std::string disp=f.value.empty()?f.placeholder:f.value;
    SDL_Color tc=f.value.empty()?m_theme.phColor:m_theme.valueColor;
    drawText(r,m_fontMd,disp,rc.x+12,rc.y+10,tc,rc.w-24);
}

// ── dropdown overlays ─────────────────────────────────────────
void GridLayout::renderDropdowns(SDL_Renderer* r) {
    for (int i=0;i<(int)m_fields.size();++i) {
        GridField& f=m_fields[i];
        if (!f.dropOpen||f.type!=FieldType::Select||f.isOtherMode) continue;

        SDL_Rect base=m_inputRects[i];
        int nOpts=(int)f.options.size();
        int totalItems=nOpts+1; // +1 for "Other..."
        int oh=totalItems*34+8;
        SDL_Rect drop={base.x, base.y+base.h+3, base.w, oh};

        drawRoundRect(r,drop,5,m_theme.inputBg);
        drawRoundRect(r,drop,5,m_theme.inputBdrFoc,false);

        // regular options
        for (int j=0;j<nOpts;++j) {
            SDL_Rect item={drop.x, drop.y+4+j*34, drop.w, 34};
            bool sel=(j==f.selectedOption);
            if (sel)
                drawRoundRect(r,{item.x+4,item.y+2,item.w-8,item.h-4},4,m_theme.accentBg);
            SDL_Color tc=sel?SDL_Color{255,255,255,255}:m_theme.valueColor;
            drawText(r,m_fontMd,f.options[j],item.x+14,item.y+10,tc,item.w-24);
        }

        // separator + "Other..." row
        int oy=drop.y+4+nOpts*34;
        drawRect(r,{drop.x+8,oy,drop.w-16,1},{60,65,100,255});
        SDL_Rect otherItem={drop.x,oy+1,drop.w,34};
        // pencil icon
        sc(r,m_theme.otherColor);
        int px=otherItem.x+14, py=otherItem.y+10;
        SDL_RenderDrawLine(r,px,py+7,px+8,py);
        SDL_RenderDrawLine(r,px+1,py+7,px+9,py);
        SDL_RenderDrawLine(r,px,py+7,px+2,py+9);
        drawText(r,m_fontMd,"Other...",otherItem.x+28,otherItem.y+10,
                 m_theme.otherColor,otherItem.w-36);
    }
}

// ── status bar ────────────────────────────────────────────────
void GridLayout::renderStatusBar(SDL_Renderer* r) {
    drawRect(r,m_statusRC,m_theme.statusBg);
    int n=(int)m_fields.size();
    int rows=(n+m_cols-1)/m_cols;
    // green dot
    drawRoundRect(r,{10,m_statusRC.y+(m_sbH-8)/2,8,8},4,m_theme.statusDot);
    std::string msg="columns="+std::to_string(m_cols)
        +" padding="+std::to_string(m_pad)+"px"
        +" widgets="+std::to_string(n)
        +" rows="+std::to_string(rows);
    drawText(r,m_fontSm,msg,24,m_statusRC.y+(m_sbH-11)/2,m_theme.statusText);
}

// ── events ────────────────────────────────────────────────────
void GridLayout::handleEvent(const SDL_Event& e) {

    if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT) {
        int mx=e.button.x, my=e.button.y;

        // ── Dropdown / Other back-arrow handling ──────────────
        bool handled=false;
        for (int i=0;i<(int)m_fields.size();++i) {
            GridField& f=m_fields[i];

            // back arrow in Other mode
            if (f.isOtherMode && f.type==FieldType::Select) {
                SDL_Rect rc=m_inputRects[i];
                SDL_Rect arrowZone={rc.x+rc.w-30,rc.y,30,rc.h};
                if (hit(mx,my,arrowZone)) {
                    f.isOtherMode=false; f.otherValue=""; f.value="";
                    f.selectedOption=-1; m_focusIdx=-1;
                    handled=true; break;
                }
            }

            if (!f.dropOpen) continue;
            SDL_Rect base=m_inputRects[i];
            int nOpts=(int)f.options.size();
            int oh=(nOpts+1)*34+8;
            SDL_Rect drop={base.x,base.y+base.h+3,base.w,oh};

            if (hit(mx,my,drop)) {
                handled=true;
                int item=(my-drop.y-4)/34;
                if (item>=0 && item<nOpts) {
                    // normal option
                    f.selectedOption=item; f.value=f.options[item];
                    f.isOtherMode=false; f.otherValue="";
                    if (m_changedCb) m_changedCb(i,f.value);
                } else if (item==nOpts) {
                    // Other... selected
                    f.isOtherMode=true; f.otherValue=""; f.value="";
                    f.selectedOption=-1; m_focusIdx=i;
                }
            }
            f.dropOpen=false;
        }
        if (handled) return;

        // ── Slider thumbs ─────────────────────────────────────
        if (hit(mx,my,m_colSlider.thumb)) { m_colSlider.dragging=true; return; }
        if (hit(mx,my,m_padSlider.thumb)) { m_padSlider.dragging=true; return; }

        // ── Align buttons ─────────────────────────────────────
        for (int i=0;i<(int)m_alignBtns.size();++i) {
            if (hit(mx,my,m_alignBtns[i].rc)) {
                m_align=(CellAlign)i;
                for (auto& b:m_alignBtns) b.active=false;
                m_alignBtns[i].active=true;
                layout(m_winW,m_winH); return;
            }
        }

        // ── + Add field ───────────────────────────────────────
        if (hit(mx,my,m_addBtn.rc)) {
            GridField nf;
            nf.label="Field "+std::to_string(m_fields.size()+1);
            nf.placeholder="Enter value…"; nf.type=FieldType::Text;
            m_fields.push_back(nf); m_origFields.push_back(nf);
            layout(m_winW,m_winH);
            if (m_addedCb) m_addedCb();
            return;
        }

        // ── − Remove ─────────────────────────────────────────
        if (hit(mx,my,m_removeBtn.rc)) {
            if (!m_fields.empty()) {
                int idx=(int)m_fields.size()-1;
                removeLastField();
                layout(m_winW,m_winH);
                if (m_removedCb) m_removedCb(idx);
            }
            return;
        }

        // ── Field focus / open dropdown ───────────────────────
        m_focusIdx=-1;
        for (int i=0;i<(int)m_fields.size();++i) {
            if (hit(mx,my,m_inputRects[i])) {
                m_focusIdx=i;
                GridField& f=m_fields[i];
                if (f.type==FieldType::Select && !f.isOtherMode)
                    f.dropOpen=!f.dropOpen;
                break;
            }
        }
    }

    if (e.type==SDL_MOUSEBUTTONUP)
        m_colSlider.dragging=m_padSlider.dragging=false;

    if (e.type==SDL_MOUSEMOTION) {
        int mx=e.motion.x;
        auto drag=[&](Slider& sl, int& prop, int mn, int mx2){
            if (!sl.dragging) return;
            float f=(float)(mx-sl.track.x)/sl.track.w;
            f=std::clamp(f,0.f,1.f);
            prop=mn+(int)std::round(f*(mx2-mn));
            sl.value=prop;
            sl.thumb.x=sl.track.x+(int)(f*sl.track.w)-sl.thumb.w/2;
            layout(m_winW,m_winH);
        };
        drag(m_colSlider,m_cols,1,5);
        drag(m_padSlider,m_pad, 0,40);
    }

    // ── Text input ────────────────────────────────────────────
    if (e.type==SDL_TEXTINPUT && m_focusIdx>=0) {
        auto& f=m_fields[m_focusIdx];
        if (f.type==FieldType::Select && f.isOtherMode) {
            f.otherValue+=e.text.text;
            f.value=f.otherValue;
            if (m_changedCb) m_changedCb(m_focusIdx,f.value);
        } else if (f.type!=FieldType::Select) {
            f.value+=e.text.text;
            if (m_changedCb) m_changedCb(m_focusIdx,f.value);
        }
    }

    if (e.type==SDL_KEYDOWN && m_focusIdx>=0) {
        auto& f=m_fields[m_focusIdx];
        if (f.type==FieldType::Select && f.isOtherMode) {
            if (e.key.keysym.sym==SDLK_BACKSPACE && !f.otherValue.empty()) {
                f.otherValue.pop_back(); f.value=f.otherValue;
                if (m_changedCb) m_changedCb(m_focusIdx,f.value);
            }
            if (e.key.keysym.sym==SDLK_ESCAPE) {
                f.isOtherMode=false; f.otherValue=""; f.value="";
                f.selectedOption=-1; m_focusIdx=-1;
            }
            if (e.key.keysym.sym==SDLK_RETURN) m_focusIdx=-1;
        } else {
            if (e.key.keysym.sym==SDLK_BACKSPACE && !f.value.empty()) {
                f.value.pop_back();
                if (m_changedCb) m_changedCb(m_focusIdx,f.value);
            }
            if (e.key.keysym.sym==SDLK_TAB)
                m_focusIdx=(m_focusIdx+1)%(int)m_fields.size();
            if (e.key.keysym.sym==SDLK_RETURN) m_focusIdx=-1;
        }
    }

    if (e.type==SDL_WINDOWEVENT &&
       (e.window.event==SDL_WINDOWEVENT_RESIZED ||
        e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED))
        layout(e.window.data1,e.window.data2);
}