#include "EditorEngineLoop.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, uint32 msg, WPARAM wParam, LPARAM lParam);

LRESULT FEditorEngineLoop::StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    FEditorEngineLoop * EditorEngineLoop = reinterpret_cast<FEditorEngineLoop *>(GetWindowLongPtr(HWnd, GWLP_USERDATA));
    
    if (Message == WM_NCCREATE)
    {
        CREATESTRUCTW * CreateStruct = reinterpret_cast<CREATESTRUCTW *>(LParam);
        EditorEngineLoop = reinterpret_cast<FEditorEngineLoop*>(CreateStruct->lpCreateParams);
        SetWindowLongPtr(HWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(EditorEngineLoop));
    }
    
    if (EditorEngineLoop)
    {
        return EditorEngineLoop->WndProc(HWnd, Message, WParam, LParam);
    }
    
    return DefWindowProc(HWnd, Message, WParam, LParam);
}

LRESULT FEditorEngineLoop::WndProc(HWND HWnd, uint32 Message, WPARAM WParam, LPARAM LParam)
{
    if (ImGui_ImplWin32_WndProcHandler(HWnd, Message, WParam, LParam))
    {
        return true;
    }

    switch (Message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProc(HWnd, Message, WParam, LParam);
}

bool FEditorEngineLoop::PreInit(HINSTANCE HInstance, uint32 NCmdShow)
{
    (void)NCmdShow;

    WCHAR     WindowClass[] = L"JungleWindowClass";
    WCHAR     Title[] = L"Game Tech Lab";
    WNDCLASSW Wndclass = {0, StaticWndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass};

    RegisterClassW(&Wndclass);

    HWindow = CreateWindowExW(
        0,
        WindowClass,
        Title,
        WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1920, 1080,
        nullptr, nullptr, HInstance, this);

    if (!HWindow)
    {
        return false;
    }
    
    return true;
}

int32 FEditorEngineLoop::Run()
{
    while (!bIsExit)
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0,PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                bIsExit = true;
                break;
            }
        }
        
        if (bIsExit)
        {
            break;
        }
        
        Tick();
    }
    
    return 0;
}

void FEditorEngineLoop::Shutdown()
{
    //  TODO : Editor Release
}

void FEditorEngineLoop::Tick()
{
    //  TODO : Call Update Functions
}