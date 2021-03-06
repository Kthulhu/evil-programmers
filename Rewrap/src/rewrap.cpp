#include <CRT/crt.hpp>
#include <plugin.hpp>
#include "version.hpp"

// {F6E77027-05BA-4ECF-A8D3-7D57B2D80C53}
static const GUID MainGuid =
{ 0xf6e77027, 0x5ba, 0x4ecf, { 0xa8, 0xd3, 0x7d, 0x57, 0xb2, 0xd8, 0xc, 0x53 } };

// {34040B7C-FE0D-401B-8862-E328BD85D857}
static const GUID MenuGuid =
{ 0x34040b7c, 0xfe0d, 0x401b, { 0x88, 0x62, 0xe3, 0x28, 0xbd, 0x85, 0xd8, 0x57 } };

enum{
    IDS_Rewrap,
    IDS_Cancel,
    IDS_NoEsc,
    IDS_OldEsc,
    IDS_NoWrap,
    IDS_ReformatParagraph
};

static const wchar_t szEsc[]=L"ESC";

FARAPIEDITORCONTROL EditorControl;
FARAPIGETMSG        GetMsg;
FARAPIMESSAGE       Message;

// configurable parameters
int nWrapPos=0;            // 0 for no wrap
int isJustifyEnabled=0;
int isWrapQuote=0;         // process quotes

void WINAPI GetGlobalInfoW(struct GlobalInfo *Info)
{
	Info->StructSize=sizeof(struct GlobalInfo);
	Info->MinFarVersion=FARMANAGERVERSION;
	Info->Version=PLUGIN_VERSION;
	Info->Guid=MainGuid;
	Info->Title=PLUGIN_NAME;
	Info->Description=PLUGIN_DESC;
	Info->Author=PLUGIN_AUTHOR;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
	EditorControl=Info->EditorControl;
	GetMsg=Info->GetMsg;
	Message=Info->Message;
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info)
{
	static const wchar_t *PluginMenuStrings[1];

	Info->StructSize=sizeof(*Info);
	Info->Flags=PF_EDITOR|PF_DISABLEPANELS;
	PluginMenuStrings[0]=GetMsg(&MainGuid,IDS_ReformatParagraph);
	Info->PluginMenu.Guids=&MenuGuid;
	Info->PluginMenu.Strings=PluginMenuStrings;
	Info->PluginMenu.Count=ARRAYSIZE(PluginMenuStrings);
}

struct EditorInfo ei = {sizeof(ei)};
wchar_t nlsSpace;

inline int IsCSpace(wchar_t ch)
{
    return ch==L' ';
}

static int IsQuote(const wchar_t* pszStr, size_t nLength)
// returns the length of initial string part which is a quote or 0 otherwize
{
    size_t i=0;
    wchar_t q=L'>';

    while( i<nLength && i<4 ){
        if( pszStr[i++]==q ){
            while( i<nLength && (pszStr[i]==q || IsCSpace(pszStr[i])) ) i++;
            return (int)i;
        }
    }
    return 0;
}

static int IsSameQuote(const wchar_t* pszQuote1, size_t nLen1, const wchar_t* pszQuote2, size_t nLen2)
{
    while( nLen1 )
        if( IsCSpace(pszQuote1[nLen1-1]) )nLen1--;
        else break;
    while( nLen2 )
        if( IsCSpace(pszQuote2[nLen2-1]) )nLen2--;
        else break;
    return nLen1==nLen2 && memcmp(pszQuote1,pszQuote2,nLen1*sizeof(wchar_t))==0;
}

HANDLE WINAPI OpenW(const struct OpenInfo *OInfo)
{
    struct EditorSetPosition esp  = {sizeof(esp)};
    struct EditorGetString egs = {sizeof(egs)};
    struct EditorSetString ess = {sizeof(ess)};
    struct EditorSelect es = {sizeof(es)};
    struct EditorUndoRedo eur = {sizeof(eur)};
    intptr_t i;
    int j;
    int nIndent1, nIndent2;
    wchar_t* pMem;
    int nLen;
    int isBlank;
    int nStart;
    int nPara;
    int nAddLine;
    div_t SpaceCount;
    static HMODULE hEsc=NULL;
    static int (WINAPI *GetEditorSettings)(intptr_t EditorID, const wchar_t *szName, void *Param);
    const wchar_t* szText[3];
    int nQuote;
    wchar_t* szQuote;
    HANDLE hHeap;

    (void)OInfo;

    if( !hEsc ){
        hEsc=GetModuleHandle(L"esc.dll");
        if( !hEsc ){
            szText[0]=GetMsg(&MainGuid,IDS_Rewrap);
            szText[1]=GetMsg(&MainGuid,IDS_NoEsc);
            szText[2]=GetMsg(&MainGuid,IDS_Cancel);
            Message(&MainGuid,NULL,FMSG_ERRORTYPE|FMSG_WARNING,szEsc,szText,3,1);
            return NULL;
        }
    }
    if( !GetEditorSettings ){
        GetEditorSettings=(int (WINAPI*)(intptr_t, const wchar_t*, void*))GetProcAddress(hEsc,"GetEditorSettingsW");
        if( !GetEditorSettings ){
            szText[0]=GetMsg(&MainGuid,IDS_Rewrap);
            szText[1]=GetMsg(&MainGuid,IDS_OldEsc);
            szText[2]=GetMsg(&MainGuid,IDS_Cancel);
            Message(&MainGuid,NULL,FMSG_ERRORTYPE|FMSG_WARNING,szEsc,szText,3,1);
            return NULL;
        }
    }

    EditorControl(-1,ECTL_GETINFO,0,&ei);

    GetEditorSettings(ei.EditorID,L"wrap",&nWrapPos);
    GetEditorSettings(ei.EditorID,L"justify",&isJustifyEnabled);
    GetEditorSettings(ei.EditorID,L"p_quote",&isWrapQuote);

    if( nWrapPos<1 || nWrapPos>512 ){
        szText[0]=GetMsg(&MainGuid,IDS_Rewrap);
        szText[1]=GetMsg(&MainGuid,IDS_NoWrap);
        szText[2]=GetMsg(&MainGuid,IDS_Cancel);
        Message(&MainGuid,NULL,FMSG_WARNING,szEsc,szText,3,1);
        return NULL;
    }

    esp.CurPos=0;
    esp.CurTabPos=-1;
    esp.TopScreenLine=-1;
    esp.LeftPos=-1;
    esp.Overtype=-1;

    if( ei.BlockType!=BTYPE_NONE ){
        esp.CurLine=ei.BlockStartLine;
        EditorControl(-1,ECTL_SETPOSITION,0,&esp);
    }

    nlsSpace=L' ';

    nIndent1=nIndent2=-1;
    pMem=NULL;
    nLen=0;
    isBlank=1;
    ess.StringLength=0;
    nAddLine=0;
    nQuote=0;
    szQuote=NULL;
    hHeap=GetProcessHeap();

    eur.Command=EUR_BEGIN;
    EditorControl(-1,ECTL_UNDOREDO,0,&eur);

    do{

        i=-1;
        EditorControl(-1,ECTL_EXPANDTABS,0,&i);

        egs.StringNumber=-1;
        EditorControl(-1,ECTL_GETSTRING,0,&egs);

        if( !pMem ){
            if( isWrapQuote ){
                nQuote=IsQuote(egs.StringText,egs.StringLength);
                if( nQuote ){
                    if( nQuote>=(nWrapPos-1) )nQuote=0;
                    else{
                        szQuote=(wchar_t*)HeapAlloc(hHeap,HEAP_GENERATE_EXCEPTIONS,(nQuote+1)*sizeof(wchar_t));
                        wmemcpy(szQuote,egs.StringText,nQuote);
                        szQuote[nQuote]=L'\0';
                    }
                }
            }else nQuote=0;
        }

        if( nQuote==0 ){
            if( nIndent1==-1 ){
                for( nIndent1=0; nIndent1<egs.StringLength; nIndent1++ )
                    if( !IsCSpace(egs.StringText[nIndent1]) ) break;
                if( ei.BlockType==BTYPE_NONE && nIndent1==egs.StringLength ){
                    esp.CurLine=ei.CurLine+1;
                    esp.CurPos=0;
                    EditorControl(-1,ECTL_SETPOSITION,0,&esp);
                    eur.Command=EUR_END;
                    EditorControl(-1,ECTL_UNDOREDO,0,&eur);
                    return NULL;
                }
            }else if( nIndent2==-1 ){
                for( nIndent2=0; nIndent2<egs.StringLength; nIndent2++ )
                    if( !IsCSpace(egs.StringText[nIndent2]) ) break;
                if( ei.BlockType==BTYPE_NONE && nIndent2==egs.StringLength ){
                    nAddLine=1;
                    break;
                }
            }else if( ei.BlockType==BTYPE_NONE ){
                for( i=0; i<egs.StringLength; i++ )
                    if( !IsCSpace(egs.StringText[i]) ) break;
                if( i==egs.StringLength ){
                    nAddLine=1;
                    break;
                }
                if( i!=nIndent2 ) break;
            }
        }else{// there is a quote in the first line...
            i=IsQuote(egs.StringText,egs.StringLength);
            if( ei.BlockType==BTYPE_NONE && (!IsSameQuote(szQuote,nQuote,egs.StringText,i)) ){
                for( j=0; j<egs.StringLength; j++ )
                    if( !IsCSpace(egs.StringText[j]) ) break;
                if( j==egs.StringLength )nAddLine=1;
                break;
            }
            egs.StringText+=i;
            egs.StringLength-=i;
        }

        if( pMem ){
            pMem=(wchar_t*)HeapReAlloc(hHeap,HEAP_GENERATE_EXCEPTIONS,pMem,(nLen+egs.StringLength+1)*sizeof(wchar_t));
            if( !isBlank ){
                pMem[nLen++]=(char)nlsSpace;
                isBlank=1;
            }
        }else
            pMem=(wchar_t*)HeapAlloc(hHeap,HEAP_GENERATE_EXCEPTIONS,egs.StringLength*sizeof(wchar_t));

        for( i=0; i<egs.StringLength; i++ ){
            if( IsCSpace(egs.StringText[i]) ){
                if( isBlank ) continue;
                else isBlank=1;
            }else isBlank=0;
            pMem[nLen++]=egs.StringText[i];
        }

        EditorControl(-1,ECTL_DELETESTRING,0,NULL);

        if( ei.BlockType!=BTYPE_NONE ){
            egs.StringNumber=-1;
            EditorControl(-1,ECTL_GETSTRING,0,&egs);
        }

    }while( (egs.SelStart>=0 && egs.SelStart!=egs.SelEnd) ||
            ei.BlockType==BTYPE_NONE
          );

    if( nLen && IsCSpace(pMem[nLen-1]) ) nLen--;

    if( nLen==0 ){
        EditorControl(-1,ECTL_INSERTSTRING,0,0);
        eur.Command=EUR_END;
        EditorControl(-1,ECTL_UNDOREDO,0,&eur);
        return NULL;
    }

    if( nIndent1>=nWrapPos-1 || nIndent1<0 ) nIndent1=0;
    if( nIndent2>=nWrapPos-1 || nIndent2<0 ) nIndent2=nIndent1;

    ess.StringText=(wchar_t*)HeapAlloc(hHeap,HEAP_GENERATE_EXCEPTIONS,(nWrapPos+2)*sizeof(wchar_t));
    nStart=0;

    EditorControl(-1,ECTL_GETINFO,0,&ei);

    while( nStart<nLen ){

        wmemset((wchar_t*)ess.StringText,nlsSpace,nIndent1);
        wmemcpy((wchar_t*)&ess.StringText[nIndent1],szQuote,nQuote);
        ess.StringLength=nPara=j=nIndent1+nQuote;
        nIndent1=nIndent2;

        for( i=nStart; i<nLen; i++ ){
            ((wchar_t*)ess.StringText)[j]=pMem[i];
            if( IsCSpace(pMem[i]) ){
                ess.StringLength=j++;
                nStart=(int)(i+1);
            }else{
                if( ++j>nWrapPos ){
                    i--;
                    break;
                }
            }
        }
        if( i==nLen ){
            nStart=(int)i;
            ess.StringLength=j;
        }else if( i<nLen ){
            if( nIndent1==ess.StringLength ){
                nIndent1=0;
                ess.StringLength=j-1;
                nStart=(int)(i+1);
            }else if( isJustifyEnabled && ess.StringLength<nWrapPos ){
                for( j=0, i=nPara; i<ess.StringLength; i++ )
                    if( IsCSpace(ess.StringText[i]) )j++;
                // j==amount of meaning blanks==word count-1
                if( j ){
                    SpaceCount.quot = nWrapPos-(int)ess.StringLength+j;
                    SpaceCount.rem = SpaceCount.quot % j;
                    SpaceCount.quot = SpaceCount.quot / j;
                    // now we have minimum space length in SpaceCount.quot
                    // and amount of blank fields with extra spacing in SpaceCount.rem
                    for( j=nWrapPos-1, i=(int)(ess.StringLength-1); i>=nPara; i-- ){
                        if( IsCSpace(ess.StringText[i]) ){
                            if( SpaceCount.rem ){
                                SpaceCount.rem--;
                                ((wchar_t*)ess.StringText)[j--]=nlsSpace;
                            }
                            j-=SpaceCount.quot;
                            wmemset( (wchar_t*)ess.StringText+j+1, nlsSpace, SpaceCount.quot );
                        }else
                            ((wchar_t*)ess.StringText)[j--]=ess.StringText[i];
                    }
                    ess.StringLength=nWrapPos;
                }
            }
        }

        if( ei.CurLine==-1 ){
            egs.StringNumber=-1;
            EditorControl(-1,ECTL_GETSTRING,0,&egs);
            esp.CurLine=-1;
            esp.CurPos=egs.StringLength;
            EditorControl(-1,ECTL_SETPOSITION,0,&esp);
        }
        EditorControl(-1,ECTL_INSERTSTRING,0,0);
        if( ei.CurLine!=-1 ){
            esp.CurLine=ei.CurLine;
            EditorControl(-1,ECTL_SETPOSITION,0,&esp);
            ei.CurLine=-1;
        }

        ess.StringNumber=-1;
        ess.StringEOL=egs.StringEOL;
        EditorControl(-1,ECTL_SETSTRING,0,&ess);

    }

    HeapFree(hHeap,0,(wchar_t*)ess.StringText);
    HeapFree(hHeap,0,szQuote);
    HeapFree(hHeap,0,pMem);

    es.BlockType=BTYPE_NONE;
    EditorControl(-1,ECTL_SELECT,0,&es);

    EditorControl(-1,ECTL_GETINFO,0,&ei);

    esp.CurLine=ei.CurLine+1+nAddLine;
    esp.CurPos=0;
    EditorControl(-1,ECTL_SETPOSITION,0,&esp);

    eur.Command=EUR_END;
    EditorControl(-1,ECTL_UNDOREDO,0,&eur);

    EditorControl(-1,ECTL_REDRAW,0,NULL);

    return NULL;
}

#if defined(__GNUC__)
#ifdef __cplusplus
extern "C"{
#endif
  BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
#ifdef __cplusplus
};
#endif

BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  (void) hDll;
  (void) dwReason;
  (void) lpReserved;
  return TRUE;
}
#endif
