#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include "Game.hpp"

namespace {
void trace(const char* message) noexcept {
    HANDLE file=CreateFileA("fu_startup.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){
        DWORD written=0;
        WriteFile(file,message,DWORD(std::strlen(message)),&written,nullptr);
        WriteFile(file,"\r\n",2,&written,nullptr);
        CloseHandle(file);
    }
}
LONG WINAPI crashLog(EXCEPTION_POINTERS* info) {
    char msg[256]{};
    sprintf_s(msg,"FATAL exception: 0x%08lx at address %p",
        info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress);
    trace(msg);
    return EXCEPTION_CONTINUE_SEARCH;
}
fu::Game game;
HINSTANCE appInstance{};
HWND mainWindow{}, mainText{}, listControl{}, inputControl{}, titleText{}, subtitleText{}, statusText{}, pitchWindow{}, clubLogoWindow{};
HWND nav[16]{}, actions[6]{};
HWND setupName{}, setupClub{}, setupRole{}, setupDifficulty{}, setupLeagues{}, setupHelp{}, setupStart{}, setupLoad{};
HFONT uiFont{}, titleFont{}, monoFont{}, smallFont{};
HBRUSH backgroundBrush{}, surfaceBrush{}, inputBrush{};
ULONG_PTR gdiplusToken{};
constexpr COLORREF background=RGB(13,21,35),surface=RGB(23,35,54),primary=RGB(57,112,221),accent=RGB(67,202,179),white=RGB(233,239,247),muted=RGB(160,174,196);
int page=-1;
bool started=false, showPitch=false;
std::vector<int> visibleIds;
std::vector<int> activeNavigation;
int lastListIndex=0;
std::wstring detailText;
const std::wstring navLabels[]={L"Обзор клуба",L"Состав",L"Тактика",L"Тренировки",L"Матч",L"Турнирная таблица",L"Трансферы",L"Контракты",L"Разговоры",L"Болельщики",L"Финансы",L"Академия",L"Почта",L"История матчей",L"Настройки",L"Помощь"};
const int NAV_START=100, ACTION_START=300, LIST_ID=501, SETUP_START=600, SETUP_LOAD=601, SETUP_CLUB=602, SETUP_LEAGUES=603;
std::string saveFile="career.fus";

std::wstring wide(const std::string& s){
    if(s.empty())return {};
    int count=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    if(count<=0)return L"[Ошибка кодировки]";
    std::wstring w(count,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),count);
    return w;
}
std::string utf8(const std::wstring& w){
    if(w.empty())return {};
    int count=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
    std::string s(count,'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),count,nullptr,nullptr);return s;
}
std::wstring getText(HWND h){
    int n=GetWindowTextLengthW(h);
    std::wstring s(size_t(n)+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;
}
void setText(HWND h,const std::string& text){
    std::wstring w=wide(text),out;
    out.reserve(w.size()+w.size()/20);
    for(size_t i=0;i<w.size();++i){
        if(w[i]==L'\n'&&(i==0||w[i-1]!=L'\r'))out+=L'\r';
        out+=w[i];
    }
    SetWindowTextW(h,out.c_str());
}
std::wstring money(int v){std::wostringstream o;o<<std::fixed<<std::setprecision(1)<<v/1000000.0<<L" млн €";return o.str();}
HWND create(const wchar_t* kind, const wchar_t* label,DWORD style,int id=0,DWORD ex=0,HWND parent=nullptr){
    HWND wnd=CreateWindowExW(ex,kind,label,style,0,0,100,30,parent?parent:mainWindow,HMENU(INT_PTR(id)),appInstance,nullptr);
    SendMessageW(wnd,WM_SETFONT,WPARAM(uiFont),TRUE);return wnd;
}
void show(HWND wnd,bool visible){if(wnd)ShowWindow(wnd,visible?SW_SHOW:SW_HIDE);}
void move(HWND wnd,int x,int y,int w,int h){if(wnd)MoveWindow(wnd,x,y,std::max(0,w),std::max(0,h),TRUE);}
void fillCombo(HWND combo,const std::vector<std::wstring>& items,int defaultItem){
    SendMessageW(combo,CB_RESETCONTENT,0,0);
    for(const auto& s:items)SendMessageW(combo,CB_ADDSTRING,0,LPARAM(s.c_str()));
    SendMessageW(combo,CB_SETCURSEL,WPARAM(defaultItem),0);
}
void addList(std::wstring title,int id){
    SendMessageW(listControl,LB_ADDSTRING,0,LPARAM(title.c_str()));visibleIds.push_back(id);
}
int selectedId(){int pos=int(SendMessageW(listControl,LB_GETCURSEL,0,0));return pos>=0&&pos<int(visibleIds.size())?visibleIds[pos]:-1;}
void textContent(const std::string& content){setText(mainText,content);SendMessageW(mainText,EM_SETSEL,0,0);SendMessageW(mainText,EM_SCROLLCARET,0,0);}
void tell(const std::string& result){setText(statusText,result);}
std::string playerDetails(int id){
    const fu::Player* p=game.findPlayer(id);
    if(!p)return "Выберите игрока из списка.";
    std::ostringstream o;
    o<<"ID: "<<p->id<<" | "<<p->name<<" / "<<p->position<<" / "<<p->age<<" лет\n";
    o<<"Статус: "<<(p->starter?"ОСНОВА":"ЗАПАС")<<"\n";
    static const char* focusNames[]={"Скорость","Удар","Пас","Отбор","Техника","Сила","Видение поля","Выносливость","Решения","Вратарское мастерство"};
    o<<"Индивидуальный фокус: "<<(p->trainingFocus<0?"Командный план":focusNames[p->trainingFocus])<<"\n\n";
    o<<"Клуб: "<<(game.findClub(p->club)?game.findClub(p->club)->name:std::string("—"))<<"\n";
    static const char* names[]={"Скорость","Удар","Пас","Отбор","Техника","Сила","Видение поля","Выносливость","Решения","Игра в воротах"};
    for(int i=0;i<10;++i)o<<names[i]<<": "<<p->skill[i]<<"/100\n";
    o<<"\nПотенциал: "<<p->potential<<"/100\n";
    o<<"Мораль: "<<p->morale<<"/100\nФорма: "<<p->fitness<<"/100\nДоверие: "<<p->trust<<"/100\n";
    o<<"Благополучие семьи: "<<p->family<<"/100\n";
    o<<"Характер: "<<p->personality<<"\n";
    o<<"Зарплата: "<<p->wage<<" € в неделю\nСтоимость: "<<p->value<<" €\n";
    o<<"Контракт до: "<<p->contractYear<<"\n";
    if(p->injured)o<<"Травма: ещё "<<p->injured<<" дней\n";
    return o.str();
}
void arrange(){
    if(!mainWindow)return;
    RECT r{};GetClientRect(mainWindow,&r);int w=std::max(920,static_cast<int>(r.right)),h=std::max(650,static_cast<int>(r.bottom));
    int left=215,right=w-24,bodyWidth=right-left;
    if(!started){
        move(titleText,45,25,w-215,48);move(subtitleText,45,78,w-215,35);
        move(clubLogoWindow,w-150,24,105,105);
        move(setupName,45,160,400,35);move(setupClub,45,215,400,300);
        move(setupRole,45,270,400,230);move(setupDifficulty,45,325,400,230);
        move(setupLeagues,470,160,std::max(300,w-515),270);
        move(setupHelp,45,387,400,150);
        move(setupStart,45,550,220,44);move(setupLoad,277,550,220,44);
        move(statusText,40,h-52,w-80,36);
    }else{
        move(titleText,left,24,bodyWidth-100,40);
        move(subtitleText,left,65,bodyWidth-100,22);
        move(clubLogoWindow,right-85,12,82,82);
        for(int i=0;i<16;++i)move(nav[i],15,75+i*37,187,34);
        bool listNeeded=page==1||page==3||page==4||page==5||page==6||page==7||page==8||page==12||page==14;
        bool pitchNeeded=(page==4&&showPitch);
        int contentY=102,contentH=std::max(300,h-284);
        if(listNeeded){
            int listWidth=std::max(255,bodyWidth*39/100);
            move(listControl,left,contentY,listWidth,contentH);
            int textX=left+listWidth+12,textW=bodyWidth-listWidth-12;
            if(pitchNeeded){
                int fieldH=std::min(240,contentH*40/100);
                move(mainText,textX,contentY,textW,contentH-fieldH-10);
                move(pitchWindow,textX,contentY+contentH-fieldH,textW,fieldH);
            }else move(mainText,textX,contentY,textW,contentH);
        }else if(pitchNeeded){
            int textW=bodyWidth*48/100;
            move(mainText,left,contentY,textW,contentH);
            move(pitchWindow,left+textW+14,contentY,bodyWidth-textW-14,300);
        }else move(mainText,left,contentY,bodyWidth,contentH);
        move(inputControl,left,h-168,bodyWidth,37);
        int buttonW=(bodyWidth-5*8)/5;
        for(int i=0;i<5;++i)move(actions[i],left+i*(buttonW+8),h-120,buttonW,39);
        move(actions[5],left,h-74,buttonW,32);
        move(statusText,left+buttonW+12,h-74,bodyWidth-buttonW-12,34);
    }
}
std::vector<std::wstring> buttonTexts(int p){
    switch(p){
        case 0:return {L"День +1",L"До матча (+7)",L"Сохранить",L"Загрузить",L"Матч"};
        case 1:return {L"В основу",L"Автосостав",L"Разговор",L"Контракт",L"Обновить"};
        case 2:return {L"Сменить схему",L"Сменить стиль",L"Тренировки",L"Матч",L"Обновить"};
        case 3:return {L"План команды",L"Фокус игрока",L"Состав",L"День +1",L"Обновить"};
        case 4:return {L"Начать матч",L"+15 минут",L"Мгновенно",L"Сменить тактику",L"2D / текст"};
        case 5:return {L"Лига клуба",L"Другая лига",L"Обновить",L"—",L"—"};
        case 6:return {L"Купить игрока",L"Обновить рынок",L"Состав",L"—",L"—"};
        case 7:return {L"Предложить",L"Состав",L"Обновить",L"—",L"—"};
        case 8:return {L"Отправить текст",L"Похвалить",L"Помочь семье",L"Обновить",L"—"};
        case 9:return {L"Обратиться",L"Поблагодарить",L"Встреча",L"Обновить",L"—"};
        case 10:return {L"Стадион",L"Трениров. база",L"Академия",L"Спонсор",L"Билет +5 €"};
        case 11:return {L"Набрать игрока",L"Развить академию",L"Состав",L"Обновить",L"—"};
        case 12:return {L"Обновить",L"Обзор",L"—",L"—",L"—"};
        case 13:return {L"Обновить",L"Матч",L"—",L"—",L"—"};
        case 14:return {L"Вкл./выкл. лигу",L"Новая карьера",L"Сохранить",L"Загрузить",L"Импорт CSV"};
        default:return {L"Обзор",L"—",L"—",L"—",L"—"};
    }
}
void drawPitch(HDC dc,RECT area){
    HBRUSH grass=CreateSolidBrush(RGB(14,75,62));FillRect(dc,&area,grass);DeleteObject(grass);
    HPEN pen=CreatePen(PS_SOLID,2,RGB(183,227,208));auto oldPen=SelectObject(dc,pen);auto oldBrush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    RECT field={16,16,area.right-16,area.bottom-16};
    Rectangle(dc,field.left,field.top,field.right,field.bottom);
    int mid=(field.left+field.right)/2, cy=(field.top+field.bottom)/2;
    MoveToEx(dc,mid,field.top,nullptr);LineTo(dc,mid,field.bottom);
    Ellipse(dc,mid-43,cy-43,mid+43,cy+43);
    Rectangle(dc,field.left,cy-72,field.left+58,cy+72);
    Rectangle(dc,field.right-58,cy-72,field.right,cy+72);
    SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
    const int xs[11]={4,17,17,17,17,36,36,36,56,56,68};
    const int ys[11]={50,16,38,62,84,22,50,78,22,78,50};
    int seed=game.hasLiveMatch()?game.live().minute:30;
    for(int team=0;team<2;++team){
        HBRUSH jersey=CreateSolidBrush(team?RGB(247,120,106):RGB(69,160,241));
        size_t count=11;
        if(game.hasLiveMatch())count=team?game.live().awayXI.size():game.live().homeXI.size();
        for(size_t i=0;i<std::min<size_t>(11,count);++i){
            int x=team?(100-xs[i]):xs[i];
            int y=ys[i];
            int offset=((seed+4*i+team*2)%11)-5;
            if(i>0)x=std::clamp(x+offset,5,95);
            int px=field.left+(field.right-field.left)*x/100;
            int py=field.top+(field.bottom-field.top)*y/100;
            HBRUSH old=HBRUSH(SelectObject(dc,jersey));Ellipse(dc,px-7,py-7,px+7,py+7);SelectObject(dc,old);
        }
        DeleteObject(jersey);
    }
    HBRUSH ball=CreateSolidBrush(RGB(255,244,150));HBRUSH old=HBRUSH(SelectObject(dc,ball));
    int bx=field.left+(field.right-field.left)*(20+(seed*7)%60)/100;
    int by=field.top+(field.bottom-field.top)*(20+(seed*3)%60)/100;
    Ellipse(dc,bx-4,by-4,bx+4,by+4);SelectObject(dc,old);DeleteObject(ball);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(220,243,227));
    SelectObject(dc,smallFont);
    const wchar_t* hint=L"Условная схема позиций (не физическая симуляция)";
    TextOutW(dc,22,area.bottom-23,hint,int(wcslen(hint)));
}
LRESULT CALLBACK pitchProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);drawPitch(dc,r);EndPaint(hwnd,&ps);return 0;}
    return DefWindowProcW(hwnd,msg,w,l);
}
// PNG logos are optional. Place files in assets/clubs/<club_id>.png next to the executable.
// Club IDs are stable in this demo database; see assets/clubs/CLUB_IDS.csv.
LRESULT CALLBACK logoProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);
        RECT r{};GetClientRect(hwnd,&r);
        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        Gdiplus::SolidBrush bg(Gdiplus::Color(13,21,35));
        graphics.FillRectangle(&bg,0,0,r.right,r.bottom);
        int clubId=0;
        if(started){ clubId=game.teamId(); }
        else if(setupClub){
            int index=int(SendMessageW(setupClub,CB_GETCURSEL,0,0));
            if(index>=0&&index<int(game.clubs().size()))clubId=game.clubs()[index].id;
        }
        const fu::Club* club=game.findClub(clubId);
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr,modulePath,MAX_PATH);
        std::filesystem::path logoPath=std::filesystem::path(modulePath).parent_path()/L"assets"/L"clubs"/(std::to_wstring(clubId)+L".png");
        bool hasLogo=false;
        if(std::filesystem::exists(logoPath)){
            Gdiplus::Bitmap image(logoPath.c_str());
            if(image.GetLastStatus()==Gdiplus::Ok&&image.GetWidth()>0&&image.GetHeight()>0){
                int inset=4;int side=std::min(r.right,r.bottom)-2*inset;
                graphics.DrawImage(&image,Gdiplus::Rect((r.right-side)/2,(r.bottom-side)/2,side,side));
                hasLogo=true;
            }
        }
        if(!hasLogo){
            int side=std::min(r.right,r.bottom)-10, cx=r.right/2,cy=r.bottom/2;
            Gdiplus::SolidBrush fill(Gdiplus::Color(255,29,61,99));
            Gdiplus::Pen outline(Gdiplus::Color(255,67,202,179),2.0f);
            graphics.FillEllipse(&fill,cx-side/2,cy-side/2,side,side);
            graphics.DrawEllipse(&outline,cx-side/2,cy-side/2,side,side);
            std::wstring label=club?wide(club->name):L"FC";
            if(label.size()>2)label=label.substr(0,2);
            Gdiplus::FontFamily family(L"Segoe UI");
            Gdiplus::Font font(&family,28.0f,Gdiplus::FontStyleBold,Gdiplus::UnitPixel);
            Gdiplus::SolidBrush ink(Gdiplus::Color(255,233,239,247));
            Gdiplus::StringFormat format;format.SetAlignment(Gdiplus::StringAlignmentCenter);format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF box(0.0f,0.0f,float(r.right),float(r.bottom));
            graphics.DrawString(label.c_str(),-1,&font,box,&format,&ink);
        }
        EndPaint(hwnd,&ps);return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
void showPage(int next);
void detailForSelected(){
    int id=selectedId();
    if(id<0){textContent("Выберите пункт из списка.");return;}
    if(page==1)textContent(playerDetails(id)+"\n\n"+game.lineupText());
    else if(page==3)textContent(game.tacticsText()+"\n"+playerDetails(id)+"\nНажмите «Фокус игрока» для переключения навыка.");
    else if(page==6||page==7||page==8)textContent(playerDetails(id));
    else if(page==5)textContent(game.standings(id));
    else if(page==12){const auto& inbox=game.inbox();if(id>=0&&id<int(inbox.size()))textContent(inbox[id].title+"\n\n"+inbox[id].body);}
    else if(page==14){const auto& l=game.leagues();if(id>=0&&id<int(l.size()))textContent(game.standings(id)+"\n\nНажмите «Вкл./выкл. лигу», чтобы переключить детализацию.\nКлубная лига всегда активна.");}
}
void populate(){
    visibleIds.clear();SendMessageW(listControl,LB_RESETCONTENT,0,0);
    if(page==1||page==3||page==4||page==7||page==8){
        for(int id:game.ownPlayers()){
            const auto* p=game.findPlayer(id);if(!p)continue;
            std::string marker=p->starter?"[ОСН] ":"[ЗАП] ";
            if(page==4&&game.hasLiveMatch()){
                const auto& live=game.live();
                const bool home=game.upcomingMatch()&&game.upcomingMatch()->home==game.teamId();
                const auto& xi=home?live.homeXI:live.awayXI;
                const auto& used=home?live.usedHome:live.usedAway;
                if(std::find(xi.begin(),xi.end(),p->id)!=xi.end())marker="[ПОЛЕ] ";
                else if(std::find(used.begin(),used.end(),p->id)!=used.end())marker="[ВЫШЕЛ] ";
                else marker="[ЗАП] ";
            }
            addList(wide("#"+std::to_string(p->id)+" "+marker+p->position+" "+p->name+(p->injured?" [ТРАВМА]":"")),id);
        }
    }else if(page==6){
        for(int id:game.transferCandidates()){
            const auto* p=game.findPlayer(id);if(!p)continue;
            addList(wide(p->position+" | "+p->name+" | "+std::to_string(p->value/1000000.0)+" млн €"),id);
        }
    }else if(page==5||page==14){
        for(const auto& l:game.leagues()){
            addList(wide(l.country+" • "+l.name+(l.active?" [АКТИВНА]":" [ФОН]")),l.id);
        }
    }else if(page==12){
        const auto& inbox=game.inbox();
        for(int j=int(inbox.size())-1;j>=0;--j)addList(wide(inbox[j].title),j);
    }
    if(!visibleIds.empty()){
        int i=std::clamp(lastListIndex,0,int(visibleIds.size())-1);
        SendMessageW(listControl,LB_SETCURSEL,i,0);
    }
}
void showPage(int next){
    if(!started)return;
    page=std::clamp(next,0,15);
    lastListIndex=0;
    bool listNeeded=page==1||page==3||page==4||page==5||page==6||page==7||page==8||page==12||page==14;
    show(listControl,listNeeded);show(inputControl,page==1||page==6||page==7||page==8||page==9||(page==4&&game.hasLiveMatch()));
    show(pitchWindow,page==4&&showPitch);
    for(int i=0;i<16;++i)show(nav[i],true);
    auto labels=buttonTexts(page);
    for(int i=0;i<5;++i){
        SetWindowTextW(actions[i],labels[i].c_str());show(actions[i],labels[i]!=L"—");
    }
    show(actions[5],page==10||(page==4&&game.hasLiveMatch()));
    SetWindowTextW(actions[5],page==4?L"Заменить":L"Билет −5 €");
    SetWindowTextW(titleText,navLabels[page].c_str());
    std::wstring sub=wide(game.myClub().name)+L"   •   "+wide(game.dateString())+L"   •   Сезон "+std::to_wstring(game.season())+L"/"+std::to_wstring((game.season()+1)%100);
    SetWindowTextW(subtitleText,sub.c_str());
    if(page==1)SetWindowTextW(inputControl,L"ID игрока основы");
    if(page==4)SetWindowTextW(inputControl,L"ID игрока с поля");
    if(page==6)SetWindowTextW(inputControl,L"10000000");
    if(page==7)SetWindowTextW(inputControl,L"30000 3");
    if(page==8)SetWindowTextW(inputControl,L"Спасибо за работу! Как у тебя дела?");
    if(page==9)SetWindowTextW(inputControl,L"Спасибо за вашу поддержку клуба!");
    populate();
    switch(page){
        case 0:textContent(game.homeText());break;
        case 1:case 3:case 5:case 6:case 7:case 8:case 12:case 14:detailForSelected();break;
        case 2:textContent(game.tacticsText());break;

        case 4:textContent(game.matchText());break;
        case 9:textContent("БОЛЕЛЬЩИКИ\n\nТекущая поддержка: "+std::to_string(game.myClub().fans)+"/100\n\nНапишите своё обращение в поле ниже.\nРеакции имеют последствия для доверия, посещаемости и отношений внутри клуба.\n\nТекст понимается через локальные правила (без облачного ИИ).");break;
        case 10:textContent(game.financesText());break;
        case 11:textContent("АКАДЕМИЯ И МОЛОДЁЖЬ\n\nУровень академии: "+std::to_string(game.myClub().academy)+"/10\n\nНабор юниоров: 350 тыс. €.\nРазвитие академии повышает уровень будущих регенов.\n\nНовые воспитанники попадают в основной список футболистов.");break;
        case 13:textContent(game.matchArchive());break;
        case 15:textContent(game.helpText());break;
    }
    tell(game.status());arrange();InvalidateRect(pitchWindow,nullptr,TRUE);InvalidateRect(clubLogoWindow,nullptr,TRUE);
}
void showSetup(){
    trace("showSetup BEGIN");
    started=false;page=-1;
    for(auto& w:nav)show(w,false);
    for(auto& w:actions)show(w,false);
    show(listControl,false);show(mainText,false);show(inputControl,false);show(pitchWindow,false);
    show(setupName,true);show(setupClub,true);show(setupRole,true);show(setupDifficulty,true);
    show(setupLeagues,true);show(setupHelp,true);show(setupStart,true);show(setupLoad,true);
    SetWindowTextW(titleText,L"FOOTBALL UNIVERSE");
    SetWindowTextW(subtitleText,L"Новая карьера • 2026/27 • C++ • Windows");
    SetWindowTextW(setupName,L"Алексей");
    std::vector<std::wstring> clubs;
    for(const auto& c:game.clubs())clubs.push_back(wide(c.name+" ("+game.leagues()[c.league].country+")"));
    fillCombo(setupClub,clubs,0);trace("showSetup clubs populated");
    fillCombo(setupRole,{L"Главный тренер",L"Спортивный директор",L"Владелец клуба",L"Президент клуба",L"Песочница (все полномочия)"},0);
    fillCombo(setupDifficulty,{L"Лёгкая",L"Реалистичная",L"Сложная",L"Свободная песочница"},1);
    SendMessageW(setupLeagues,LB_RESETCONTENT,0,0);
    for(const auto& l:game.leagues())SendMessageW(setupLeagues,LB_ADDSTRING,0,LPARAM(wide(l.country+" — "+l.name).c_str()));
    for(int i=0;i<6;++i)SendMessageW(setupLeagues,LB_SETSEL,i<2,i);
    SetWindowTextW(setupHelp,L"Укажите имя, клуб, должность, сложность.\nВ списке справа отметьте активные лиги (Ctrl/Shift не нужны).\nЛига выбранного клуба включится автоматически.");
    tell("Демо-база: известные клубы, процедурные игроки. Полная лицензированная база ещё не добавлена.");
    trace("showSetup before arrange");show(clubLogoWindow,true);arrange();InvalidateRect(clubLogoWindow,nullptr,TRUE);trace("showSetup END");
}
void startCareer(){
    int club=int(SendMessageW(setupClub,CB_GETCURSEL,0,0));
    int role=int(SendMessageW(setupRole,CB_GETCURSEL,0,0));
    int diff=int(SendMessageW(setupDifficulty,CB_GETCURSEL,0,0));
    std::vector<int> active;
    for(int i=0;i<int(game.leagues().size());++i)if(SendMessageW(setupLeagues,LB_GETSEL,i,0)>0)active.push_back(i);
    game.newCareer(utf8(getText(setupName)),club,fu::Role(std::clamp(role,0,4)),fu::Difficulty(std::clamp(diff,0,3)),active);
    started=true;
    show(setupName,false);show(setupClub,false);show(setupRole,false);show(setupDifficulty,false);
    show(setupLeagues,false);show(setupHelp,false);show(setupStart,false);show(setupLoad,false);
    show(mainText,true);show(clubLogoWindow,true);showPage(0);
}
void trigger(int action){
    int id=selectedId();
    std::string text=utf8(getText(inputControl));
    std::string response;
    auto go=[&](int p){showPage(p);};
    if(page==0){
        if(action==0)game.advance(1);
        if(action==1)game.advance(7);
        if(action==2)response=game.save(saveFile);
        if(action==3)response=game.load(saveFile);
        if(action==4){go(4);return;}
    }else if(page==1){
        if(action==0){
            try{response=game.swapStarter(id,std::stoi(text));}
            catch(...){response="Введите числовой ID игрока, которого убираем из основы.";}
        }
        if(action==1)response=game.autoPickLineup();
        if(action==2){go(8);return;}
        if(action==3){go(7);return;}
    }else if(page==2){
        if(action==0)game.changeFormation();
        if(action==1)game.changeTactic();
        if(action==2){go(3);return;}
        if(action==3){go(4);return;}
    }else if(page==3){
        if(action==0)game.changeTraining();
        if(action==1)response=game.cycleTrainingFocus(id);
        if(action==2){go(1);return;}
        if(action==3)game.advance(1);
    }else if(page==4){
        if(action==0)game.startLiveMatch();
        if(action==1)game.stepLiveMatch(15);
        if(action==2)game.instantMatch();
        if(action==3)game.changeTactic();
        if(action==4)showPitch=!showPitch;
        if(action==5){
            try{response=game.substitute(std::stoi(text),id);}
            catch(...){response="Введите числовой ID футболиста с поля и выберите запасного слева.";}
        }
    }else if(page==5){
        if(action==0){id=game.myClub().league;lastListIndex=id;}
        if(action==1)lastListIndex=(lastListIndex+1)%int(game.leagues().size());
    }else if(page==6){
        if(action==0){
            try{response=game.transferBid(id,std::stoi(text));}
            catch(...){response="Введите сумму предложения в евро, например 10000000";}
        }
        if(action==2){go(1);return;}
    }else if(page==7){
        if(action==0){
            std::istringstream in(text);int wage=0,years=0;
            if(in>>wage>>years)response=game.renewContract(id,wage,years);
            else response="Введите зарплату и срок: 30000 3";
        }
        if(action==1){go(1);return;}
    }else if(page==8){
        if(action==0)response=game.talk(id,text);
        if(action==1)response=game.talk(id,"Молодец! Я ценю твою работу.");
        if(action==2)response=game.talk(id,"Как дела в семье? Давай поможем с переездом и адаптацией.");
    }else if(page==9){
        if(action==0)response=game.speakToFans(text);
        if(action==1)response=game.speakToFans("Спасибо всем за поддержку нашего клуба!");
        if(action==2)response=game.speakToFans("Приглашаю лидеров болельщиков на встречу. Давайте обсудим проблемы и поддержку.");
    }else if(page==10){
        if(action==0)response=game.spendMoney("stadium");
        if(action==1)response=game.spendMoney("training");
        if(action==2)response=game.spendMoney("academy");
        if(action==3)response=game.negotiateSponsor();
        if(action==4)response=game.adjustTickets(+5);
        if(action==5)response=game.adjustTickets(-5);
    }else if(page==11){
        if(action==0)response=game.recruitYouth();
        if(action==1)response=game.spendMoney("academy");
        if(action==2){go(1);return;}
    }else if(page==12){if(action==1){go(0);return;}}
    else if(page==13){if(action==1){go(4);return;}}
    else if(page==14){
        if(action==0)game.toggleLeague(id);
        if(action==1){showSetup();return;}
        if(action==2)response=game.save(saveFile);
        if(action==3)response=game.load(saveFile);
        if(action==4)response=game.importPlayers("data/players.csv")?"CSV загружен":"Не удалось загрузить data/players.csv";
    }else if(page==15){if(action==0){go(0);return;}}
    int old=lastListIndex;
    showPage(page);
    if(!visibleIds.empty()&&old<int(visibleIds.size())){
        lastListIndex=old;
        SendMessageW(listControl,LB_SETCURSEL,lastListIndex,0);detailForSelected();
    }
    if(!response.empty())tell(response);
}
void drawButton(const DRAWITEMSTRUCT* dis){
    HDC dc=dis->hDC;RECT r=dis->rcItem;
    bool navButton=dis->CtlID>=NAV_START&&dis->CtlID<NAV_START+16;
    bool primaryButton=(dis->CtlID>=ACTION_START&&dis->CtlID<ACTION_START+6)||(dis->CtlID==SETUP_START);
    COLORREF color=navButton?(page==int(dis->CtlID-NAV_START)?RGB(28,65,117):RGB(16,27,45)):
        (primaryButton?primary:RGB(32,52,81));
    if(dis->itemState&ODS_SELECTED)color=RGB(40,84,132);
    HBRUSH brush=CreateSolidBrush(color);
    FillRect(dc,&r,brush);DeleteObject(brush);
    wchar_t text[256];GetWindowTextW(dis->hwndItem,text,256);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,white);SelectObject(dc,uiFont);
    DrawTextW(dc,text,-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_END_ELLIPSIS);
    if(dis->itemState&ODS_FOCUS){RECT focus=r;InflateRect(&focus,-3,-3);DrawFocusRect(dc,&focus);}
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE:{
        trace("WM_CREATE BEGIN");mainWindow=hwnd;
        titleText=create(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT);
        subtitleText=create(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT);
        SendMessageW(titleText,WM_SETFONT,WPARAM(titleFont),TRUE);
        SendMessageW(subtitleText,WM_SETFONT,WPARAM(smallFont),TRUE);
        statusText=create(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT);
        SendMessageW(statusText,WM_SETFONT,WPARAM(smallFont),TRUE);
        for(int i=0;i<16;++i)nav[i]=create(L"BUTTON",navLabels[i].c_str(),WS_CHILD|BS_OWNERDRAW,NAV_START+i);
        mainText=create(L"EDIT",L"",WS_CHILD|WS_VSCROLL|ES_LEFT|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,502,WS_EX_CLIENTEDGE);
        SendMessageW(mainText,WM_SETFONT,WPARAM(monoFont),TRUE);
        listControl=create(L"LISTBOX",L"",WS_CHILD|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,LIST_ID,WS_EX_CLIENTEDGE);
        SendMessageW(listControl,WM_SETFONT,WPARAM(smallFont),TRUE);
        inputControl=create(L"EDIT",L"",WS_CHILD|ES_AUTOHSCROLL|WS_TABSTOP,503,WS_EX_CLIENTEDGE);
        for(int i=0;i<6;++i)actions[i]=create(L"BUTTON",L"",WS_CHILD|BS_OWNERDRAW|WS_TABSTOP,ACTION_START+i);
        pitchWindow=CreateWindowExW(0,L"FU_Pitch",L"",WS_CHILD,0,0,10,10,hwnd,nullptr,appInstance,nullptr);
        clubLogoWindow=CreateWindowExW(0,L"FU_ClubLogo",L"",WS_CHILD|WS_VISIBLE,0,0,10,10,hwnd,nullptr,appInstance,nullptr);
        setupName=create(L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|WS_TABSTOP,610,WS_EX_CLIENTEDGE);
        setupClub=create(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,SETUP_CLUB);
        setupRole=create(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,611);
        setupDifficulty=create(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,612);
        setupLeagues=create(L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_MULTIPLESEL|LBS_NOINTEGRALHEIGHT,SETUP_LEAGUES,WS_EX_CLIENTEDGE);
        setupHelp=create(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT);
        setupStart=create(L"BUTTON",L"Создать карьеру",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,SETUP_START);
        setupLoad=create(L"BUTTON",L"Загрузить",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,SETUP_LOAD);
        SendMessageW(setupLeagues,WM_SETFONT,WPARAM(smallFont),TRUE);
        trace("WM_CREATE before setup");showSetup();trace("WM_CREATE END");return 0;
    }
    case WM_SIZE:arrange();return 0;
    case WM_COMMAND:{
        int id=LOWORD(w), code=HIWORD(w);
        if(id>=NAV_START&&id<NAV_START+16&&code==BN_CLICKED){showPage(id-NAV_START);return 0;}
        if(id>=ACTION_START&&id<ACTION_START+6&&code==BN_CLICKED){trigger(id-ACTION_START);return 0;}
        if(id==LIST_ID&&code==LBN_SELCHANGE){lastListIndex=int(SendMessageW(listControl,LB_GETCURSEL,0,0));detailForSelected();return 0;}
        if(id==SETUP_CLUB&&code==CBN_SELCHANGE){InvalidateRect(clubLogoWindow,nullptr,TRUE);return 0;}
        if(id==SETUP_START&&code==BN_CLICKED){startCareer();return 0;}
        if(id==SETUP_LOAD&&code==BN_CLICKED){
            std::string result=game.load(saveFile);
            if(result.find("загружена")!=std::string::npos){
                started=true;show(setupName,false);show(setupClub,false);show(setupRole,false);show(setupDifficulty,false);
                show(setupLeagues,false);show(setupHelp,false);show(setupStart,false);show(setupLoad,false);
                show(mainText,true);show(clubLogoWindow,true);showPage(0);
            }tell(result);return 0;
        }
        return 0;
    }
    case WM_DRAWITEM:{auto* dis=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(dis->CtlType==ODT_BUTTON){drawButton(dis);return TRUE;}break;}
    case WM_CTLCOLORSTATIC:{
        HDC dc=HDC(w);SetTextColor(dc,white);SetBkColor(dc,background);SetBkMode(dc,TRANSPARENT);
        return LRESULT(backgroundBrush);
    }
    case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{
        HDC dc=HDC(w);SetTextColor(dc,white);SetBkColor(dc,surface);return LRESULT(inputBrush);
    }
    case WM_ERASEBKGND:{
        RECT rect;GetClientRect(hwnd,&rect);FillRect(HDC(w),&rect,backgroundBrush);
        RECT sidebar={0,0,205,rect.bottom};HBRUSH brush=CreateSolidBrush(RGB(10,17,29));FillRect(HDC(w),&sidebar,brush);DeleteObject(brush);return 1;
    }
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int showCommand){
    SetUnhandledExceptionFilter(crashLog);trace("wWinMain START");
    appInstance=instance;
    SetProcessDPIAware();
    Gdiplus::GdiplusStartupInput gdiplusInput;
    if(Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusInput,nullptr)!=Gdiplus::Ok)return 1;
    trace("GDI+ STARTED");uiFont=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    smallFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    titleFont=CreateFontW(-33,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    monoFont=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    backgroundBrush=CreateSolidBrush(background);surfaceBrush=CreateSolidBrush(surface);inputBrush=CreateSolidBrush(surface);
    trace("fonts STARTED");WNDCLASSEXW pitchClass{};pitchClass.cbSize=sizeof(pitchClass);pitchClass.hInstance=instance;pitchClass.lpfnWndProc=pitchProc;
    pitchClass.lpszClassName=L"FU_Pitch";pitchClass.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassExW(&pitchClass);
    WNDCLASSEXW logoClass{};logoClass.cbSize=sizeof(logoClass);logoClass.hInstance=instance;logoClass.lpfnWndProc=logoProc;
    logoClass.lpszClassName=L"FU_ClubLogo";logoClass.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassExW(&logoClass);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=windowProc;
    wc.lpszClassName=L"FootballUniverseDesktop";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);
    wc.hbrBackground=backgroundBrush;RegisterClassExW(&wc);
    trace("classes REGISTERED");HWND window=CreateWindowExW(0,wc.lpszClassName,L"Football Universe — C++ Prototype 0.3",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1240,840,nullptr,nullptr,instance,nullptr);
    if(!window){MessageBoxW(nullptr,L"Не удалось создать окно",L"Ошибка",MB_ICONERROR);return 1;}
    trace("window CREATED");ShowWindow(window,showCommand);trace("window SHOWN");UpdateWindow(window);trace("window UPDATED");
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    DeleteObject(uiFont);DeleteObject(titleFont);DeleteObject(monoFont);DeleteObject(smallFont);
    DeleteObject(backgroundBrush);DeleteObject(surfaceBrush);DeleteObject(inputBrush);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return int(msg.wParam);
}
#endif
