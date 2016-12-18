// vlcwin.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "vlcwin.h"

#include <Shellapi.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define UNICODE
#include <vlc/vlc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#if !defined(UNDER_CE)
# ifndef _WIN32_IE
#   define  _WIN32_IE 0x501
# endif
#   include <shlobj.h>
#   include <tlhelp32.h>
#   include <wininet.h>
# ifndef _WIN64
static void check_crashdump(void);
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo);
# endif
#if 0
typedef enum _HEAP_INFORMATION_CLASS {
        HeapCompatibilityInformation,
        HeapEnableTerminationOnCorruption
} HEAP_INFORMATION_CLASS;
#endif
WINBASEAPI BOOL WINAPI HeapSetInformation(HANDLE,HEAP_INFORMATION_CLASS,PVOID,SIZE_T);
#define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS)1
#endif

#ifndef UNDER_CE
static char *FromWide (const wchar_t *wide)
{
    size_t len;
    len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);

    char *out = (char *)malloc (len);
    if (out)
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}
#else
static int parse_cmdline (char *line, char ***argvp)
{
    char **argv = malloc (sizeof (char *));
    int argc = 0;

    while (*line != '\0')
    {
        char quote = 0;

        /* Skips white spaces */
        while (strchr ("\t ", *line))
            line++;
        if (!*line)
            break;

        /* Starts a new parameter */
        argv = realloc (argv, (argc + 2) * sizeof (char *));
        if (*line == '"')
        {
            quote = '"';
            line++;
        }
        argv[argc++] = line;

    more:
            while (*line && !strchr ("\t ", *line))
            line++;

    if (line > argv[argc - 1] && line[-1] == quote)
        /* End of quoted parameter */
        line[-1] = 0;
    else
        if (*line && quote)
    {
        /* Space within a quote */
        line++;
        goto more;
    }
    else
        /* End of unquoted parameter */
        if (*line)
            *line++ = 0;
    }
    argv[argc] = NULL;
    *argvp = argv;
    return argc;
}
#endif

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    int argc;
#ifndef UNDER_CE
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    HINSTANCE h_Kernel32 = LoadLibraryW(L"kernel32.dll");
    if(h_Kernel32)
    {
        BOOL (WINAPI * mySetProcessDEPPolicy)( DWORD dwFlags);
        BOOL (WINAPI * mySetDllDirectoryA)(const char* lpPathName);
# define PROCESS_DEP_ENABLE 1

        mySetProcessDEPPolicy = (BOOL (__stdcall *)(DWORD))			// sunqueen modify
                            GetProcAddress(h_Kernel32, "SetProcessDEPPolicy");
        if(mySetProcessDEPPolicy)
            mySetProcessDEPPolicy(PROCESS_DEP_ENABLE);

        /* Do NOT load any library from cwd. */
        mySetDllDirectoryA = (BOOL (__stdcall *)(const char *)) GetProcAddress(h_Kernel32, "SetDllDirectoryA");			// sunqueen modify
        if(mySetDllDirectoryA)
            mySetDllDirectoryA("");

        FreeLibrary(h_Kernel32);
    }

    wchar_t **wargv = CommandLineToArgvW (GetCommandLine (), &argc);
    if (wargv == NULL)
        return 1;

//    char *argv[argc + 1];
    char **argv = (char **)malloc((argc + 1) * sizeof(char *));			// sunqueen modify
    BOOL crash_handling = TRUE;
    int j = 0;

    argv[j++] = FromWide( L"--no-ignore-config" );
    for (int i = 1; i < argc; i++)
    {
        if(!wcscmp(wargv[i], L"--no-crashdump"))
        {
            crash_handling = FALSE;
        }
        else
        {
            argv[j] = FromWide (wargv[i]);
            j++;
        }
    }

    argc = j;
    argv[argc] = NULL;
    LocalFree (wargv);

# ifndef _WIN64
    if(crash_handling)
    {
        check_crashdump();
        SetUnhandledExceptionFilter(vlc_exception_filter);
    }
# endif /* WIN64 */

#else
    char **argv, psz_cmdline[wcslen(lpCmdLine) * 4];

    WideCharToMultiByte( CP_UTF8, 0, lpCmdLine, -1,
                         psz_cmdline, sizeof (psz_cmdline), NULL, NULL );

    argc = parse_cmdline (psz_cmdline, &argv);
#endif

    /* Initialize libvlc */
    libvlc_instance_t *vlc;
    vlc = libvlc_new (argc, (const char **)argv);
    if (vlc != NULL)
    {
        libvlc_add_intf (vlc, "globalhotkeys,none");
        libvlc_add_intf (vlc, NULL);
        libvlc_playlist_play (vlc, -1, 0, NULL);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }

    for (int i = 0; i < argc; i++)
        free (argv[i]);
	free(argv);			// sunqueen add

    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return 0;
}

#if !defined( UNDER_CE ) && !defined( _WIN64 )

static void get_crashdump_path(wchar_t * wdir)
{
    if( S_OK != SHGetFolderPathW( NULL,
                        CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                        NULL, SHGFP_TYPE_CURRENT, wdir ) )
        fprintf( stderr, "Can't open the vlc conf PATH\n" );

    swprintf( wdir+wcslen( wdir ), L"%s", L"\\vlc\\crashdump" );
}

static void check_crashdump()
{
    wchar_t * wdir = (wchar_t *)malloc(sizeof(wchar_t)*MAX_PATH);
    get_crashdump_path(wdir);

    FILE * fd = _wfopen ( wdir, L"r, ccs=UTF-8" );
    if( fd )
    {
        fclose( fd );
        int answer = MessageBox( NULL, L"VLC media player just crashed. Do you want to send a bug report to the developers team?",
        L"VLC crash reporting", MB_YESNO);			// sunqueen modify

        if(answer == IDYES)
        {
            HINTERNET Hint = InternetOpen(L"VLC Crash Reporter", INTERNET_OPEN_TYPE_PRECONFIG, NULL,NULL,0);
            if(Hint)
            {
                HINTERNET ftp = InternetConnect(Hint, L"crash.videolan.org", INTERNET_DEFAULT_FTP_PORT,
                                                NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);
                if(ftp)
                {
                    SYSTEMTIME now;
                    GetSystemTime(&now);
                    wchar_t remote_file[MAX_PATH];
                    swprintf( remote_file, L"/crashes-win32/%04d%02d%02d%02d%02d%02d",
                            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond );

                    if( FtpPutFile( ftp, wdir, remote_file, FTP_TRANSFER_TYPE_BINARY, 0) )
                        MessageBox( NULL, L"Report sent correctly. Thanks a lot for the help.",
                                    L"Report sent", MB_OK);
                    else
                        MessageBox( NULL, L"There was an error while transferring to the FTP server. Thanks a lot for the help anyway.",
                                    L"Report sending failed", MB_OK);			// sunqueen modify
                    InternetCloseHandle(ftp);
                }
                else
                {
                    MessageBox( NULL, L"There was an error while connecting to the FTP server. Thanks a lot for the help anyway.",
                                    L"Report sending failed", MB_OK);			// sunqueen modify
                    fprintf(stderr,"Can't connect to FTP server%d\n",GetLastError());
                }
                InternetCloseHandle(Hint);
            }
            else
            {
                  MessageBox( NULL, L"There was an error while connecting to Internet. Thanks a lot for the help anyway.",
                                    L"Reporting sending failed", MB_OK);			// sunqueen modify
            }
        }

        _wremove(wdir);
    }
    free((void *)wdir);
}

/*****************************************************************************
 * vlc_exception_filter: handles unhandled exceptions, like segfaults
 *****************************************************************************/
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo)
{
    if(IsDebuggerPresent())
    {
        //If a debugger is present, pass the exception to the debugger with EXCEPTION_CONTINUE_SEARCH
        return EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
        fprintf( stderr, "unhandled vlc exception\n" );

        wchar_t * wdir = (wchar_t *)malloc(sizeof(wchar_t)*MAX_PATH);
        get_crashdump_path(wdir);
        FILE * fd = _wfopen ( wdir, L"w, ccs=UTF-8" );
        free((void *)wdir);

        if( !fd )
        {
            fprintf( stderr, "\nerror while opening file" );
            exit( 1 );
        }

        OSVERSIONINFO osvi;
        ZeroMemory( &osvi, sizeof(OSVERSIONINFO) );
        osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
        GetVersionEx( &osvi );

        fwprintf( fd, L"[version]\nOS=%d.%d.%d.%d.%s\nVLC=%s", osvi.dwMajorVersion,
                                                               osvi.dwMinorVersion,
                                                               osvi.dwBuildNumber,
                                                               osvi.dwPlatformId,
                                                               osvi.szCSDVersion,
                                                               _T(VERSION_MESSAGE));			// sunqueen modify

        const CONTEXT *const pContext = (const CONTEXT *)lpExceptionInfo->ContextRecord;
        const EXCEPTION_RECORD *const pException = (const EXCEPTION_RECORD *)lpExceptionInfo->ExceptionRecord;
        /*No nested exceptions for now*/
        fwprintf( fd, L"\n\n[exceptions]\n%08x at %08x",pException->ExceptionCode,
                                                pException->ExceptionAddress );
        if( pException->NumberParameters > 0 )
        {
            unsigned int i;
            for( i = 0; i < pException->NumberParameters; i++ )
                fwprintf( fd, L" | %08x", pException->ExceptionInformation[i] );
        }

        fwprintf( fd, L"\n\n[context]\nEDI:%08x\nESI:%08x\nEBX:%08x\nEDX:%08x\nECX:%08x\nEAX:%08x\nEBP:%08x\nEIP:%08x\nESP:%08x\n",
                        pContext->Edi,pContext->Esi,pContext->Ebx,
                        pContext->Edx,pContext->Ecx,pContext->Eax,
                        pContext->Ebp,pContext->Eip,pContext->Esp );			// sunqueen modify

        fwprintf( fd, L"\n[stacktrace]\n#EIP|base|module\n" );

        wchar_t module[ 256 ];
        MEMORY_BASIC_INFORMATION mbi ;
        VirtualQuery( (DWORD *)pContext->Eip, &mbi, sizeof( mbi ) ) ;
        HINSTANCE hInstance = (HINSTANCE)mbi.AllocationBase;			// sunqueen modify
        GetModuleFileName( hInstance, module, 256 ) ;
        fwprintf( fd, L"%08x|%s\n", pContext->Eip, module );

        DWORD pEbp = pContext->Ebp;
        DWORD caller = *((DWORD*)pEbp + 1);

        unsigned i_line = 0;
        do
        {
            VirtualQuery( (DWORD *)caller, &mbi, sizeof( mbi ) ) ;
            HINSTANCE hInstance = (HINSTANCE)mbi.AllocationBase;			// sunqueen modify
            GetModuleFileName( hInstance, module, 256 ) ;
            fwprintf( fd, L"%08x|%s\n", caller, module );
            pEbp = *(DWORD*)pEbp ;
            caller = *((DWORD*)pEbp + 1) ;
            i_line++;
            /*The last EBP points to NULL!*/
        }while(caller && i_line< 100);

        fclose( fd );
        fflush( stderr );
        exit( 1 );
    }
}
#endif
