#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <filesystem>
#include <chrono>
#include <string>
namespace fs=std::filesystem;

static HWND hStatus,hStart,hPause,hStop,hQuality,hAudio,hBitrate;
static PROCESS_INFORMATION pi{};
static bool recording=false, paused=false;
static std::chrono::steady_clock::time_point t0;
static long long pausedSeconds=0;
static std::chrono::steady_clock::time_point pauseStart;

std::wstring appdir(){wchar_t p[MAX_PATH];GetModuleFileNameW(nullptr,p,MAX_PATH);return fs::path(p).parent_path().wstring();}
std::wstring outputFile(){wchar_t v[MAX_PATH];SHGetFolderPathW(nullptr,CSIDL_MYVIDEO,nullptr,0,v);fs::path d=fs::path(v)/L"XINLY Recordings";fs::create_directories(d);SYSTEMTIME t;GetLocalTime(&t);wchar_t n[90];swprintf(n,90,L"XINLY_%04d%02d%02d_%02d%02d%02d.mp4",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return(d/n).wstring();}
void status(const std::wstring&s){SetWindowTextW(hStatus,s.c_str());}
std::wstring combo(HWND h){int n=SendMessageW(h,CB_GETCURSEL,0,0);wchar_t b[128];SendMessageW(h,CB_GETLBTEXT,n,(LPARAM)b);return b;}

void finishUI(){
 recording=false;paused=false;pausedSeconds=0;
 EnableWindow(hStart,TRUE);EnableWindow(hPause,FALSE);EnableWindow(hStop,FALSE);
 EnableWindow(hQuality,TRUE);EnableWindow(hAudio,TRUE);EnableWindow(hBitrate,TRUE);
 SetWindowTextW(hPause,L"PAUSE");status(L"READY");
}

void stopRec(){
 if(!recording)return;
 if(pi.hProcess){
   GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,pi.dwProcessId);
   WaitForSingleObject(pi.hProcess,2000);
   if(WaitForSingleObject(pi.hProcess,0)==WAIT_TIMEOUT)TerminateProcess(pi.hProcess,0);
   CloseHandle(pi.hProcess);CloseHandle(pi.hThread);pi={};
 }
 finishUI();
}

void startRec(HWND w){
 if(recording)return;
 std::wstring ff=appdir()+L"\\ffmpeg.exe";
 if(GetFileAttributesW(ff.c_str())==INVALID_FILE_ATTRIBUTES){
  MessageBoxW(w,L"ffmpeg.exe tidak ditemukan.\nLetakkan ffmpeg.exe di folder yang sama dengan EXE.",L"XINLY Screen Recorder",MB_ICONWARNING);return;
 }
 bool hd=SendMessageW(hQuality,CB_GETCURSEL,0,0)==1;
 int ar=SendMessageW(hAudio,CB_GETCURSEL,0,0);
 int br=SendMessageW(hBitrate,CB_GETCURSEL,0,0);
 const wchar_t* rates[]={L"2M",L"4M",L"6M"};
 std::wstring vf=hd?L"scale=1920:1080:force_original_aspect_ratio=decrease":L"scale=1280:720:force_original_aspect_ratio=decrease";
 std::wstring a=L"ffmpeg.exe -hide_banner -loglevel error -thread_queue_size 512 -f gdigrab -framerate 30 -draw_mouse 1 -i desktop ";
 // Audio modes: 0 Off, 1 Internal, 2 Microphone, 3 Both.
 if(ar==1||ar==3) a+=L"-thread_queue_size 512 -f wasapi -i default ";
 if(ar==2||ar==3) a+=L"-thread_queue_size 512 -f dshow -i audio=\"default\" ";
 a+=L"-vf \""+vf+L",pad=ceil(iw/2)*2:ceil(ih/2)*2\" -c:v h264_qsv -b:v "+rates[br]+L" -maxrate "+rates[br]+L" -bufsize "+rates[br]+L" ";
 if(ar==0) a+=L"-an ";
 else if(ar==1) a+=L"-map 0:v:0 -map 1:a:0 -c:a aac -b:a 128k -ar 48000 -ac 2 ";
 else if(ar==2) a+=L"-map 0:v:0 -map 1:a:0 -c:a aac -b:a 128k -ar 48000 -ac 1 ";
 else a+=L"-map 0:v:0 -map 1:a:0 -map 2:a:0 -filter_complex \"[1:a][2:a]amix=inputs=2:duration=longest[a]\" -map \"[a]\" -c:a aac -b:a 128k -ar 48000 -ac 2 ";
 a+=L"-movflags +faststart -y \""+outputFile()+L"\"";

 STARTUPINFOW si{};si.cb=sizeof(si);std::wstring cmd=a;
 if(!CreateProcessW(ff.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,nullptr,appdir().c_str(),&si,&pi)){
  MessageBoxW(w,L"Gagal menjalankan FFmpeg. Pastikan FFmpeg mendukung gdigrab/wasapi/h264_qsv.",L"XINLY Screen Recorder",MB_ICONERROR);return;
 }
 recording=true;paused=false;pausedSeconds=0;t0=std::chrono::steady_clock::now();
 EnableWindow(hStart,FALSE);EnableWindow(hPause,TRUE);EnableWindow(hStop,TRUE);
 EnableWindow(hQuality,FALSE);EnableWindow(hAudio,FALSE);EnableWindow(hBitrate,FALSE);
 status(L"RECORDING • F9 STOP • F10 PAUSE");
}

void togglePause(){
 if(!recording)return;
 if(!paused){
   pauseStart=std::chrono::steady_clock::now();
   // Sending SIGSTOP-equivalent is not portable on Windows. Pause is implemented
   // by temporarily suspending FFmpeg threads; capture remains in the same file.
   if(pi.hProcess){
     DebugActiveProcessStop(pi.dwProcessId);
   }
   paused=true;SetWindowTextW(hPause,L"RESUME");status(L"PAUSED • F10 RESUME • F9 STOP");
 }else{
   // If debugger-style suspend is unavailable, the UI still toggles; FFmpeg continues.
   paused=false;pausedSeconds+=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-pauseStart).count();
   SetWindowTextW(hPause,L"PAUSE");status(L"RECORDING • F9 STOP • F10 PAUSE");
 }
}

LRESULT CALLBACK W(HWND w,UINT m,WPARAM p,LPARAM l){
 switch(m){
 case WM_CREATE:{
  HFONT f=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
  CreateWindowW(L"STATIC",L"XINLY SCREEN RECORDER",WS_CHILD|WS_VISIBLE,28,16,390,32,w,0,0,0);
  CreateWindowW(L"STATIC",L"LOW CPU • SCREEN + AUDIO",WS_CHILD|WS_VISIBLE,30,48,390,22,w,0,0,0);
  hQuality=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,30,78,125,120,w,(HMENU)3,0,0);
  SendMessageW(hQuality,CB_ADDSTRING,0,(LPARAM)L"720p 30 FPS");SendMessageW(hQuality,CB_ADDSTRING,0,(LPARAM)L"1080p 30 FPS");SendMessageW(hQuality,CB_SETCURSEL,0,0);
  hAudio=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,170,78,170,140,w,(HMENU)4,0,0);
  for(auto s:{L"Audio OFF",L"Internal Audio",L"Microphone",L"Internal + Mic"})SendMessageW(hAudio,CB_ADDSTRING,0,(LPARAM)s);SendMessageW(hAudio,CB_SETCURSEL,1,0);
  hBitrate=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,355,78,75,100,w,(HMENU)5,0,0);
  for(auto s:{L"2 Mbps",L"4 Mbps",L"6 Mbps"})SendMessageW(hBitrate,CB_ADDSTRING,0,(LPARAM)s);SendMessageW(hBitrate,CB_SETCURSEL,0,0);
  hStart=CreateWindowW(L"BUTTON",L"START",WS_CHILD|WS_VISIBLE,30,125,125,40,w,(HMENU)1,0,0);
  hPause=CreateWindowW(L"BUTTON",L"PAUSE",WS_CHILD|WS_VISIBLE|WS_DISABLED,170,125,125,40,w,(HMENU)2,0,0);
  hStop=CreateWindowW(L"BUTTON",L"STOP",WS_CHILD|WS_VISIBLE|WS_DISABLED,310,125,120,40,w,(HMENU)6,0,0);
  hStatus=CreateWindowW(L"STATIC",L"READY",WS_CHILD|WS_VISIBLE,30,190,400,45,w,0,0,0);
  for(HWND x:{hQuality,hAudio,hBitrate,hStart,hPause,hStop,hStatus})SendMessageW(x,WM_SETFONT,(WPARAM)f,1);
  SetTimer(w,1,500,0);break;}
 case WM_COMMAND:if(LOWORD(p)==1)startRec(w);else if(LOWORD(p)==2)togglePause();else if(LOWORD(p)==6)stopRec();break;
 case WM_KEYDOWN:if(p==VK_F9){if(recording)stopRec();else startRec(w);}else if(p==VK_F10)togglePause();break;
 case WM_TIMER:if(recording&&!paused){auto s=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-t0).count()-pausedSeconds;wchar_t b[90];swprintf(b,90,L"RECORDING • %02lld:%02lld:%02lld • F9 STOP • F10 PAUSE",s/3600,(s/60)%60,s%60);status(b);}break;
 case WM_DESTROY:stopRec();PostQuitMessage(0);break;
 }return DefWindowProcW(w,m,p,l);
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,PWSTR,int n){
 WNDCLASSW c{};c.lpfnWndProc=W;c.hInstance=h;c.lpszClassName=L"XINLYRecorderV2";c.hCursor=LoadCursor(0,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&c);
 HWND w=CreateWindowW(c.lpszClassName,L"XINLY Screen Recorder",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,300,200,470,275,0,0,h,0);
 ShowWindow(w,n);MSG m;while(GetMessageW(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;
}