/*
    [ESC] Editor's settings changer plugin for FAR Manager
    Copyright (C) 2001 Ivan Sintyurin
    Copyright (C) 2008 Alex Yaroslavsky

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#define STRICT
#include "myrtl.hpp"
#include "plugin.hpp"

#if defined(__GNUC__)
extern "C"
{
  BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
};

BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  (void) hDll;
  (void) dwReason;
  (void) lpReserved;
  return TRUE;
}
#endif

#ifndef FARMACRO_KEY_EVENT
  #define FARMACRO_KEY_EVENT (KEY_EVENT|0x8000)
#endif

#include "KeySequenceStorage.hpp"
#include "rbtree.cpp"
#include "TArray.cpp"
#include "CUserMacros.hpp"
#include "esc.hpp"
#include "mix.hpp"
#include "SaveAndLoadConfig.hpp"
#include "CIndicator.hpp"
#include "syslog.hpp"
#ifdef _DEBUG
extern wchar_t SysLogName[MAX_PATH*2];
#endif

#include "xmlite.hpp"

extern bool prc_Minuses;

#ifdef _check_mem
extern DWORD _check_mem_DAT;
#endif

extern wchar_t GLOBAL_EOL[10];
extern HANDLE heapNew_ESC;

#ifdef __cplusplus
void *operator new(size_t size)
{
  size = size ? size : 1;
  return malloc(size);
}

void *operator new[] (size_t size)
{
  return::operator new(size);
}
void *operator new(size_t /*size */ , void *p)
{
  return p;
}
void operator delete(void *p)
{
  free(p);
}
void operator delete[] (void *ptr)
{
  ::operator delete(ptr);
}
#endif // __cplusplus

BOOL WINAPI CompLT(const EditorSettingsStorage &a,const EditorSettingsStorage &b)
{
  return a.EditorId<b.EditorId;
}

BOOL WINAPI CompEQ(const EditorSettingsStorage &a,const EditorSettingsStorage &b)
{
  return a.EditorId==b.EditorId;
}

BOOL WINAPI ESCFICompLT(const ESCFileInfo &a,const ESCFileInfo &b)
{
  return (a.Name.str && b.Name.str)?wstricmp(a.Name.str, b.Name.str)<0:0;
}

BOOL WINAPI ESCFICompEQ(const ESCFileInfo &a,const ESCFileInfo &b)
{
  return a.Name==b.Name;
}

BOOL InitSetsTree(const EditorInfo &EI)
{
   if(!NodeData)
     return FALSE;
   BOOL Ret=FALSE;
   EditorSettingsStorage ESS;
   Node_<EditorSettingsStorage> *Item;
   int TotalNodes=NodeData->getSize();
   NODEDATA *nd;
   int LastAcceptableType=-1;
   for(int i=TotalNodes-1;i>=0;--i)
   {
     nd=NodeData->getItem(i);
     if(CmpWithFileMask(nd->mask.str, EI.FileName, nd->Options&E_SkipPath_On))
     {
       LastAcceptableType=i;
       if(nd->Options&E_OverLoad_On)
         break;
     }
   }

   if(LastAcceptableType>=0)
   {
     nd=NodeData->getItem(LastAcceptableType);
     ESS.EditorId=EI.EditorID;
     ESS.index=LastAcceptableType;
     Item=ESETStorage->findNode(ESS);
     if(Item)
       Item->data.Data=*nd;
     else
     {
       ESS.Data=*nd;
       ESETStorage->insertNode(ESS);
     }

     Ret=TRUE;
   }
   _D(SysLog(L"IST: Ret=%d", Ret));
   return Ret;
}

void FreeMem()
{
  if(NodeData)
  {
    delete NodeData;
    NodeData=NULL;
  }
  XMLLoadedOK=FALSE;
  if(ESETStorage)
    ESETStorage->Empty();
}

void WINAPI _export SetStartupInfoW(const struct PluginStartupInfo *Info)
{
  ::Info=*Info;
  EditorControl=::Info.EditorControl;
  CharTable=::Info.CharTable;
  ModuleNumber=::Info.ModuleNumber;
  FarMessage=::Info.Message;
  IsOldFar=TRUE;
  XMLLoadedOK=FALSE;
  NodeData=NULL;
  wstrcpy(PluginRootKey,Info->RootKey);
  wstrcat(PluginRootKey,L"\\ESC");
  _D(wsprintf(SysLogName, L"%s.LOG", ::Info.ModuleName););
  _D(SysLog(L"SetStartupInfo - start"));
  heapNew_ESC = HeapCreate(0, 0, 0);
#ifdef _check_mem
  _check_mem_DAT = 0;
#endif
  if(Info->StructSize >= (int)sizeof(struct PluginStartupInfo) && heapNew_ESC)
  {
    ::FSF=*Info->FSF;
    ::Info.FSF=&::FSF;
    TruncPathStr=FSF.TruncPathStr;
    wstrcpy(XMLFilePath, ::Info.ModuleName);
    int offset=wstrlen(XMLFilePath)-3;
    wstrcpy(XMLFilePath+offset, L"XML");

    DWORD __FarVer=0;
    ::Info.AdvControl(ModuleNumber, ACTL_GETFARVERSION, &__FarVer);

#ifndef UNICODE
    IsOldFar=!(HIBYTE(LOWORD(__FarVer))>1 ||
      (HIBYTE(LOWORD(__FarVer))==1 && LOBYTE(LOWORD(__FarVer))>70) ||
      (HIBYTE(LOWORD(__FarVer))==1 && LOBYTE(LOWORD(__FarVer))==70 &&
       HIWORD(__FarVer)>=2087));
#else
    IsOldFar=!(HIBYTE(LOWORD(__FarVer))>1 ||
      (HIBYTE(LOWORD(__FarVer))==1 && LOBYTE(LOWORD(__FarVer))>80) ||
      (HIBYTE(LOWORD(__FarVer))==1 && LOBYTE(LOWORD(__FarVer))==80 &&
       HIWORD(__FarVer)>=635));
#endif
    LoadGlobalConfig();

    nlsStopChars=new strcon;
    ESETStorage=new CRedBlackTree<EditorSettingsStorage>(CompLT, CompEQ);
    FileInfoTree=new CRedBlackTree<ESCFileInfo>(ESCFICompLT, ESCFICompEQ);
    if(!nlsStopChars || !ESETStorage || !FileInfoTree)
      IsOldFar=TRUE;

    int OldSet=Opt.ReloadSettingsAutomatically;
    Opt.ReloadSettingsAutomatically=TRUE;
    ReloadSettings(TRUE);
    Opt.ReloadSettingsAutomatically=OldSet;
  }
  _D(SysLog(L"SetStartupInfo - end (%d)", IsOldFar));
}

HANDLE WINAPI _export OpenPluginW(int /*OpenFrom*/,int /*Item*/)
{
    ReloadSettings(FALSE);
    if(!IsOldFar)
    {
        DWORD TotalNodes=XMLLoadedOK?NodeData->getSize():0;
        DWORD FMISize=(1+TotalNodes)*sizeof(FarMenuItem);
        FarMenuItem *fmi=static_cast<FarMenuItem*>(malloc(FMISize));
        if(fmi)
        {
            EditorControl(ECTL_GETINFO, &ei);
            if(ESETStorage->IsEmpty())
               InitSetsTree(ei);
            memset(fmi, 0, FMISize);
            wchar_t Buf[120], Mask[120], fmt1[32], fmt2[32];

            int MaxNameSize=0, len, MaxMaskSize=0,
                RealPos=-1; // ��� �⠢��� �����, �᫨ "-1", � �����
            NODEDATA *nd;
            int LastAcceptableType=-1;
            for(int f=TotalNodes-1;f>=0;--f)
            {
              nd=NodeData->getItem(f);
              len=nd->Name.getLength();
              if(MaxNameSize<len) MaxNameSize=len;
              if(RealPos==-1 && CmpWithFileMask(nd->mask.str, ei.FileName, nd->Options&E_SkipPath_On))
              {
                LastAcceptableType=f;
                if(nd->Options&E_OverLoad_On)
                  RealPos=LastAcceptableType;
              }
            }
            if(RealPos<0)
              RealPos=LastAcceptableType;

            if(Opt.ShowFileMaskInMenu)
            {
               if(!MaxNameSize) MaxNameSize=15;
               else
               {
                  len=((ei.WindowSizeX>110)?100:ei.WindowSizeX)/2-8;
                  if(MaxNameSize>len) MaxNameSize=len;
               }
               len=(ei.WindowSizeX>110)?100:(ei.WindowSizeX-18);
               MaxMaskSize=len-MaxNameSize;
               FSF.sprintf(fmt1, L"&%%c. %%%ds \x2502 %%s", MaxNameSize);
               FSF.sprintf(fmt2, L"   %%%ds \x2502 %%s", MaxNameSize);
            }
            else
            {
               int m=ei.WindowSizeX-16;
               if(MaxNameSize>110) MaxNameSize=110;
               if(!MaxNameSize || MaxNameSize>m) MaxNameSize=m;
               FSF.sprintf(fmt1, L"&%%c. %%%ds", MaxNameSize);
               FSF.sprintf(fmt2, L"   %%%ds", MaxNameSize);
            }

            if(Opt.ShowFileMaskInMenu) for (DWORD f = 0; f < TotalNodes; ++f)
            {
                nd=NodeData->getItem(f);
                wstrncpy(Buf, nd->Name.str, sizeof(Buf)/sizeof(wchar_t)-1);
                wstrncpy(Mask, nd->mask.str, sizeof(Mask)/sizeof(wchar_t)-1);
                TruncFromRigth(Buf, MaxNameSize, TRUE);
                TruncFromRigth(Mask, MaxMaskSize, FALSE);
//FIXME                if(f<10) FSF.sprintf(fmi[f].Text,fmt1,L'0'+f,Buf,Mask);
//                else if(f<36) FSF.sprintf(fmi[f].Text,fmt1,55+f,Buf,Mask) ;//55='A'-10
//                else FSF.sprintf(fmi[f].Text,fmt2, Buf, Mask);
            }
            else for (DWORD f = 0; f < TotalNodes; ++f)
            {
                wstrncpy(Buf, NodeData->getItem(f)->Name.str, sizeof(Buf)/sizeof(wchar_t)-1);
                TruncFromRigth(Buf, MaxNameSize, TRUE);
//FIXME                if(f<10) FSF.sprintf(fmi[f].Text,fmt1,L'0'+f,Buf);
//                else if(f<36) FSF.sprintf(fmi[f].Text,fmt1,55+f,Buf) ;//55='A'-10
//                else FSF.sprintf(fmi[f].Text,fmt2, Buf);
            }

            EditorSettingsStorage ESS(ei.EditorID);
            Node_<EditorSettingsStorage> *Item=ESETStorage->findNode(ESS);
            int MenuCode=Item?Item->data.index:0, NewMenuCode;
            if(MenuCode<0 || MenuCode>=(int)TotalNodes)
                MenuCode=0;
            fmi[MenuCode].Selected=TRUE;
            if(RealPos>-1) fmi[RealPos].Checked=L'*';
            NewMenuCode = Info.Menu(ModuleNumber, -1, -1, 0,
                                    FMENU_AUTOHIGHLIGHT | FMENU_WRAPMODE | FMENU_CHANGECONSOLETITLE,
                                    GetMsg(MTitle), NULL, NULL,
                                    NULL, NULL, fmi, TotalNodes);
            if(NewMenuCode>=0 && (!Item || (Item && NewMenuCode!=MenuCode)))
            {
               if(Item)
               {
                  Item->data.Data.LockFile.Off();
                  Item->data.index=NewMenuCode;
                  Item->data.Data=*NodeData->getItem(NewMenuCode);
               }
               else
               {
                  ESS.index=NewMenuCode;
                  ESS.Data=*NodeData->getItem(NewMenuCode);
                  Item=ESETStorage->insertNode(ESS);
               }

               if(Item)
                 ApplyEditorOptions(Item->data.Data,ei.FileName);
            }
            free(fmi);
        }
    }
    EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
    return INVALID_HANDLE_VALUE;
}

void WINAPI _export ExitFARW(void)
{
  FreeMem();
  delete ESETStorage;
  ESETStorage=NULL;
  delete FileInfoTree;
  FileInfoTree=NULL;
  delete nlsStopChars;
  nlsStopChars=NULL;
#ifdef _check_mem
  _D(SysLog(L"_check_mem_DAT=%d", _check_mem_DAT));
#endif //_check_mem
  if (heapNew_ESC)
    HeapDestroy(heapNew_ESC);
}

long WINAPI ConfigDlgProc (HANDLE hDlg, int Msg, int Param1, long Param2)
{
  switch (Msg)
    {
      case DN_BTNCLICK:
      {
        switch (Param1)
        {
           case 1:
             if(Param2) // ����稫� ������
             {
               Info.SendDlgMessage(hDlg,DM_ENABLE,2,TRUE);
               Info.SendDlgMessage(hDlg,DM_ENABLE,3,TRUE);
               Info.SendDlgMessage(hDlg,DM_ENABLE,5,TRUE);
               if(BSTATE_UNCHECKED==Info.SendDlgMessage(hDlg,DM_GETCHECK,2,0))
                 Info.SendDlgMessage(hDlg,DM_ENABLE,6,TRUE);
             }
             else       // �몫�稫� ������
             {
               Info.SendDlgMessage(hDlg,DM_ENABLE,2,FALSE);
               Info.SendDlgMessage(hDlg,DM_ENABLE,3,FALSE);
               Info.SendDlgMessage(hDlg,DM_ENABLE,5,FALSE);
               Info.SendDlgMessage(hDlg,DM_ENABLE,6,FALSE);
             }
             return TRUE;
           case 2:
             Info.SendDlgMessage(hDlg,DM_ENABLE,6,Param2?FALSE:TRUE);
           case 3:
             return TRUE;
           default:
             return FALSE;
        }
      }
    }
  return Info.DefDlgProc (hDlg, Msg, Param1, Param2);
}

int WINAPI ConfigureW(int /*ItemNumber*/)
{
  if(IsOldFar) return FALSE;

  struct InitDialogItem InitItems[] = {
/*00*/ {DI_DOUBLEBOX, 3, 1, 71, 9, 0, 0, 0, 0, (wchar_t*)MTitle},
/*01*/ {DI_CHECKBOX, 5, 2, 0, 0, 0, 0, 0, 0, (wchar_t*)MTurnOnPluginModule},
/*02*/ {DI_CHECKBOX, 5, 3, 0, 0, 0, 0, 0, 0, (wchar_t*)MReloadSettingsAutomatically},
/*03*/ {DI_CHECKBOX, 5, 4, 0, 0, 0, 0, 0, 0, (wchar_t*)MShowFileMaskInMenu},
/*04*/ {DI_TEXT, 0, 5, 0, 0, 0, 0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, L""},
/*05*/ {DI_BUTTON, 0, 6, 0, 0, 0, 0, DIF_CENTERGROUP, 0,(wchar_t*)MCheckOutSettings},
/*06*/ {DI_BUTTON, 0, 6, 0, 0, 0, 0, DIF_CENTERGROUP, 0,(wchar_t*)MReloadSettings},
/*07*/ {DI_TEXT, 0, 7, 0, 0, 0, 0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, L""},
/*08*/ {DI_BUTTON, 0, 8, 0, 0, 0, 0, DIF_CENTERGROUP, 1,(wchar_t*)MOk},
/*09*/ {DI_BUTTON, 0, 8, 0, 0, 0, 0, DIF_CENTERGROUP, 0,(wchar_t*)MCancel},
  };
  struct FarDialogItem DialogItems[10];
  int ExitCode, oldTurnOnPluginModule=Opt.TurnOnPluginModule;

  InitDialogItems(InitItems, DialogItems, 10);

  //���������� �������
  DialogItems[1].Selected = Opt.TurnOnPluginModule;
  DialogItems[2].Selected = Opt.ReloadSettingsAutomatically;
  DialogItems[3].Selected = Opt.ShowFileMaskInMenu;

  if(DialogItems[2].Selected)
    DialogItems[6].Flags|=DIF_DISABLE;

  if(!DialogItems[1].Selected)
  {
    DialogItems[2].Flags|=DIF_DISABLE;
    DialogItems[3].Flags|=DIF_DISABLE;
    DialogItems[5].Flags|=DIF_DISABLE;
    DialogItems[6].Flags|=DIF_DISABLE;
  }

  for(;;)
  {
    //����� ������� FIXME
//    ExitCode = Info.DialogEx (ModuleNumber, -1, -1, 75, 11,
//                              NULL, DialogItems, 10, 0, 0, ConfigDlgProc, 0);
    if(8==ExitCode)
      {
         Opt.TurnOnPluginModule=DialogItems[1].Selected;
         Opt.ReloadSettingsAutomatically=DialogItems[2].Selected;
         Opt.ShowFileMaskInMenu=DialogItems[3].Selected;
         SaveGlobalConfig();
         if(!Opt.TurnOnPluginModule)
           FreeMem();
         else if(Opt.TurnOnPluginModule!=oldTurnOnPluginModule)
           ReloadSettings(TRUE);
         break;
      }
    else if(5==ExitCode)
         TestCfgFiles();
    else if(6==ExitCode)
      {
         ReloadSettings(TRUE);
         if(XMLLoadedOK)
           FarAllInOneMessage(GetMsg(MReloaded), 0);
      }
    else
         break;

  }
  return TRUE;
}

void WINAPI _export GetPluginInfoW(struct PluginInfo *Info)
{
  if(IsOldFar) return;
  Info->StructSize=sizeof(*Info);
  Info->Flags=PF_PRELOAD|PF_DISABLEPANELS|PF_EDITOR;

  static wchar_t *PluginConfigStrings[1], *PluginMenuStrings[1];

  PluginConfigStrings[0]=
  PluginMenuStrings[0]=const_cast<wchar_t*>(GetMsg(MTitle));
  Info->PluginMenuStringsNumber=sizeof(PluginMenuStrings)/sizeof(PluginMenuStrings[0]);
  Info->PluginMenuStrings=PluginMenuStrings;
  Info->PluginConfigStrings=PluginConfigStrings;
  Info->PluginConfigStringsNumber=sizeof(PluginConfigStrings)/sizeof(PluginConfigStrings[0]);
}

int WINAPI _export ProcessEditorEventW(int Event, void *Param)
{
  /*#ifdef _DEBUG
  wchar_t *EES[]={"EE_READ","EE_SAVE","EE_REDRAW","EE_CLOSE"};
  if(Event>=EE_READ && Event<=EE_CLOSE)
    SysLog(L"PEE: Event: %s", EES[Event]);
  else
    SysLog(L"PEE: Event: Unknown (%d)", Event);
  #endif*/
  if(ESETStorage)
  {
   if(Event==EE_REDRAW ||
      ((Event==EE_READ || Event==EE_SAVE) && ReloadSettings(FALSE)))
   {
    EditorControl(ECTL_GETINFO, &ei);
    if(Event==EE_READ) InitSetsTree(ei);
    EditorSettingsStorage ESS(ei.EditorID);
    Node_<EditorSettingsStorage> *Item=ESETStorage->findNode(ESS);
    if(Item)
    {
        NODEDATA &nodedata=Item->data.Data;
        if(Event==EE_SAVE)
        {
          bool RestorePos=FALSE, IsLocked=ei.CurState&ECSTATE_LOCKED;

          if(!IsLocked && (nodedata.Options&E_KillSpaces_On || nodedata.EOL))
          {
             _D(SysLog(L"PEE: 0. TotalLines=%d", ei.TotalLines));

             prc_Minuses=nodedata.Options&E_Process_Minuses_On;

             CIndicator Indicator(NULL);
             Indicator.StartTimer(1000);
             Indicator.SetParams(GetMsg(MWorking), ei.TotalLines);

             RestorePos=TRUE;
             InitNLS(ei, nodedata);
             InitESPandEGS(esp, egs);
             int EscBreak=FALSE, CheckEsc=0;
             void (*StrPrc)();
             if(nodedata.Options&E_KillSpaces_On && nodedata.EOL)
                StrPrc=KillSpacesAndChangeEOL;
             else if(nodedata.Options&E_KillSpaces_On)
                StrPrc=KillSpaces;
             else
                StrPrc=ChangeEOL;
             UnpackEOL(nodedata.EOL, GLOBAL_EOL);
             for(esp.CurLine=0; esp.CurLine<ei.TotalLines; ++esp.CurLine)
             {
                ++CheckEsc;
                if (CheckEsc>512)
                  {
                    Indicator.ShowProgress(esp.CurLine);
                    if (CheckForEsc())
                      {
                        if (!FarAllInOneMessage(GetMsg(MMsgStop), FMSG_MB_YESNO))
                          {
                            EscBreak=TRUE;
                            break;
                          }
                      }
                    CheckEsc=0;
                  }

                StrPrc();
             }

             if(!EscBreak) Indicator.ShowFinal();
          }

          if(!IsLocked && nodedata.Options&E_KillEmptyLines_On)
          {
             RestorePos=TRUE;
             InitESPandEGS(esp, egs);
             int LastNotEmptyLine, LimitLine, lineFound=FALSE, lEmpty;

             LastNotEmptyLine=LimitLine=(nodedata.Options&E_ForceKillEL_On)?
                                         0:ei.CurLine;

             esp.CurLine=ei.TotalLines-1;
             EditorControl(ECTL_SETPOSITION,&esp);
             EditorControl(ECTL_GETSTRING,&egs);
             lEmpty=!*egs.StringEOL;
             if(egs.StringLength>0)
             {
               LastNotEmptyLine=esp.CurLine;
               lineFound=TRUE;
             }
             else for(esp.CurLine=ei.TotalLines-2; esp.CurLine>=LimitLine;
                  --esp.CurLine)
             {
                EditorControl(ECTL_SETPOSITION,&esp);
                EditorControl(ECTL_GETSTRING,&egs);
                if(egs.StringLength>0)
                {
                  LastNotEmptyLine=esp.CurLine;
                  lineFound=TRUE;
                  break;
                }
             }

             if(!lineFound) --LastNotEmptyLine;
             for(esp.CurLine=ei.TotalLines-1-lEmpty;
                 esp.CurLine>LastNotEmptyLine; --esp.CurLine)
             {
                EditorControl(ECTL_SETPOSITION,&esp);
                EditorControl(ECTL_DELETESTRING,NULL);
             }
          }

          {
            CEditorOptions eo(nodedata);
            eo.ApplyOption(ESPT_SAVEFILEPOSITION);
          }

          if(RestorePos) // ����⠭���� ����� ������:
             RestorePosition(ei);
          nodedata.LockFile.Off();
        }
        else if(Event==EE_REDRAW)
        {
          if(nodedata.Options&E_LockFile_On && !nodedata.LockFile.IsOn())
            nodedata.LockFile.On(ei.FileName);
        }
        else  // EE_READ
        {
          ApplyEditorOptions(nodedata,ei.FileName);
          KeySequence macro;
          const KeySequenceStorage *KSS;
          BOOL stop=TRUE;
          if(!FileExists(ei.FileName)) // 䠩�� ��� �� ��᪥, ����� �� ����
          {
            EditorSetParameter espar;
            memset(&espar, 0, sizeof(espar));
            espar.Type=ESPT_CHARTABLE;
            bool done=false;
            if(*nodedata.TableName.str)
            {
              espar.Param.iParam=FindCodeTable(nodedata.TableName.str);
              if(espar.Param.iParam>-1)
              {
                espar.Param.iParam+=3;
                EditorControl(ECTL_SETPARAM, &espar);
                done=true;
              }
            }
            if(!done && nodedata.Table>0)
            {
              espar.Param.iParam=nodedata.Table;
              EditorControl(ECTL_SETPARAM, &espar);
            }

            _D(SysLog(L"PEE: auto-macros / OnCreate - start"));
            nodedata.OnCreateMacros.toBegin();
            do
            {
              KSS=nodedata.OnCreateMacros.getItem();
              if(KSS)
              {
                macro=KSS->GetSequence(stop);
                FARPostMacro(&macro);
              }
              nodedata.OnCreateMacros.toNext();
            } while(KSS && !stop);
            _D(SysLog(L"PEE: auto-macros / OnCreate - end"));
          }
          _D(SysLog(L"PEE: auto-macros / OnLoad - start"));
          nodedata.OnLoadMacros.toBegin();
          do
          {
            KSS=nodedata.OnLoadMacros.getItem();
            if(KSS)
            {
              macro=KSS->GetSequence(stop);
              FARPostMacro(&macro);
            }
            nodedata.OnLoadMacros.toNext();
          } while(KSS && !stop);
          _D(SysLog(L"PEE: auto-macros / OnLoad - end"));
        }
    }
   }
   else if(Event==EE_CLOSE)
   {
     EditorSettingsStorage ESS(*(int *)Param);
     Node_<EditorSettingsStorage> *Item=ESETStorage->findNode(ESS);
     if(Item) ESETStorage->deleteNode(Item);
   }
  }
//  _D(SysLog(L"PEE: end"));
  return 0;
}

int WINAPI _export ProcessEditorInputW(const INPUT_RECORD *Rec)
{
    static int isReenter=0, Lines, CoordX, RetCode;

    if(IsOldFar || isReenter ||
       (Rec->EventType!=KEY_EVENT && Rec->EventType!=FARMACRO_KEY_EVENT && Rec->EventType!=MOUSE_EVENT))
    {
      //_D(SysLog(L"PEI: 1 return"));
      return 0;
    }

    if((Rec->EventType==KEY_EVENT || Rec->EventType==FARMACRO_KEY_EVENT) && !Rec->Event.KeyEvent.bKeyDown)
    {
      //_D(SysLog(L"PEI: 2 return"));
      return 0;
    }

    if(Rec->EventType==MOUSE_EVENT &&
       (Rec->Event.MouseEvent.dwEventFlags&DOUBLE_CLICK ||
        !(Rec->Event.MouseEvent.dwButtonState&(FROM_LEFT_1ST_BUTTON_PRESSED|
          RIGHTMOST_BUTTON_PRESSED|FROM_LEFT_2ND_BUTTON_PRESSED))
       )
      )
    {
      //_D(SysLog(L"PEI: 3 return"));
      return 0;
    }

    _D(SysLog(L"PEI: EventType=%s",
       (Rec->EventType==KEY_EVENT)?L"KEY_EVENT":
         ((Rec->EventType==FARMACRO_KEY_EVENT)?L"FARMACRO_KEY_EVENT":
           (Rec->EventType==MOUSE_EVENT)?L"MOUSE_EVENT":L"UNKNOWN"
         )
       )
      );

    EditorControl(ECTL_GETINFO,&ei);
    if(ESETStorage->IsEmpty())
       InitSetsTree(ei);
    EditorSettingsStorage ESS(ei.EditorID);
    Node_<EditorSettingsStorage> *Item=ESETStorage->findNode(ESS);
    if(NULL==Item)
    {
      _D(SysLog(L"PEI: 4 return"));
      return 0;
    }
    NODEDATA &nodedata=Item->data.Data;

    if(Rec->EventType==MOUSE_EVENT) // ��ࠡ�⪠ ����ᮢ
    {
      if(nodedata.Options&E_UserMacro_On)
      {
         const MOUSE_EVENT_RECORD &MouseEvent=Rec->Event.MouseEvent;
         UserMacroID id;
         if(MouseEvent.dwButtonState&FROM_LEFT_1ST_BUTTON_PRESSED)
           id.ButtonState|=EBS_LClick_On;
         if(MouseEvent.dwButtonState&FROM_LEFT_2ND_BUTTON_PRESSED)
           id.ButtonState|=EBS_MClick_On;
         if(MouseEvent.dwButtonState&RIGHTMOST_BUTTON_PRESSED)
           id.ButtonState|=EBS_RClick_On;

         if(MouseEvent.dwControlKeyState&RIGHT_CTRL_PRESSED)
           id.ControlState|=ECS_RCtrl_On;
         if(MouseEvent.dwControlKeyState&LEFT_CTRL_PRESSED)
           id.ControlState|=ECS_LCtrl_On;
         if(MouseEvent.dwControlKeyState&RIGHT_ALT_PRESSED)
           id.ControlState|=ECS_RAlt_On;
         if(MouseEvent.dwControlKeyState&LEFT_ALT_PRESSED)
           id.ControlState|=ECS_LAlt_On;
         if(MouseEvent.dwControlKeyState&SHIFT_PRESSED)
           id.ControlState|=ECS_Shift_On;

         BOOL Stop;
         if(EditorPostMacro(nodedata.MouseMacros,id,ei,Stop))
         {
           _D(SysLog(L"PEI: 5 return"));
           return Stop?1:0;
         }
      }
      _D(SysLog(L"PEI: 6 return"));
      return 0;
    }

    if(Rec->EventType!=KEY_EVENT && Rec->EventType!=FARMACRO_KEY_EVENT)
    {
      _D(SysLog(L"PEI: 7 return"));
      return 0;
    }

    const KEY_EVENT_RECORD &KeyEvent=Rec->Event.KeyEvent;

    // ��ࠡ�⪠ ����ᮢ
    // ������ �� ⮫쪮 ��� KEY_EVENT, �⮡� �� �맢��� ��横������� (�맮� ����� �� �����)
    if(nodedata.Options&E_UserMacro_On && Rec->EventType==KEY_EVENT)
    {
       UserMacroID id(FSF.FarInputRecordToKey(Rec));
       BOOL Stop;
       if(EditorPostMacro(nodedata.KeyMacros,id,ei,Stop))
       {
         _D(SysLog(L"PEI: 8 return"));
         return Stop?1:0;
       }
    }

    if((KeyEvent.dwControlKeyState &
          (LEFT_ALT_PRESSED|LEFT_CTRL_PRESSED|RIGHT_ALT_PRESSED|
           RIGHT_CTRL_PRESSED)
       ) ||
       (KeyEvent.wVirtualKeyCode!=VK_END &&
        KeyEvent.wVirtualKeyCode!=VK_TAB &&
        KeyEvent.wVirtualKeyCode!=VK_BACK &&
        KeyEvent.wVirtualKeyCode!=VK_HOME &&
        KeyEvent.wVirtualKeyCode!=VK_RETURN &&
        KeyEvent.wVirtualKeyCode!=VK_DELETE &&
        (
          KeyEvent.dwControlKeyState&ENHANCED_KEY ||
          ((BYTE)KeyEvent.uChar.AsciiChar)<32
        )
       )
      )
    {
      _D(SysLog(L"PEI: 9 return"));
      return 0; // �� ��譥� ��ᥪ��� �ࠧ�
                // ॠ���㥬 ⮫쪮 �� ����� ᨬ���� � end/shift-end
    }

    int isWrapQuote=(nodedata.Options&E_ProcessQuote_On)?1:0;
    bool IsLocked=ei.CurState&ECSTATE_LOCKED;

    // wVirtualKeyCode, ��ࠡ�⪠ ������ �� ������ �� IsLocked
    if(KeyEvent.wVirtualKeyCode==VK_HOME)
    {
      RetCode=0;
      if(!(KeyEvent.dwControlKeyState & SHIFT_PRESSED) &&
         nodedata.Options & E_SmartHome_On)
      {
         isReenter=1;
         RetCode=GotoHome(ei, nodedata);
         if(RetCode)
           EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
         isReenter=0;
      }
      _D(SysLog(L"PEI: 9 return"));
      return RetCode;
    }

    // wVirtualKeyCode, ��ࠡ�⪠ ������ ���筮 ������ �� IsLocked
    if(KeyEvent.wVirtualKeyCode==VK_TAB)
    {
      RetCode=0;
      isReenter=1;
      if(nodedata.Lines && (nodedata.Options&E_SmartTab_On) &&
         (ei.Options&EOPT_EXPANDALLTABS)
        )
      {
        Lines=nodedata.Lines;
        InitESPandEGS(esp, egs);
        InitNLS(ei, nodedata);
        if(KeyEvent.dwControlKeyState & SHIFT_PRESSED) // shift-tab
        {
          CoordX=GetPrevCoordX(ei, Lines, nlsStopChars->str);
          if(CoordX>-1)
          {
            esp.CurPos=CoordX;
            RetCode=ChangeCoordX(ei, esp);
            if(RetCode && !(ei.Options&EOPT_PERSISTENTBLOCKS))
              EditorUnmarkBlock();
          }
        }
        else if(!IsLocked)                             // tab
        {
          if(ei.BlockType!=BTYPE_NONE && !(ei.Options&EOPT_PERSISTENTBLOCKS))
          {
            EditorControl(ECTL_DELETEBLOCK,NULL);
            EditorControl(ECTL_GETINFO,&ei);
          }
          CoordX=GetNextCoordX(ei, Lines, nlsStopChars->str)-ei.CurPos;
          //_D(SysLog(L"SmartTab=%d", CoordX));
          if(CoordX>-1)
          {
            EditorControl(ECTL_GETSTRING, &egs);
            ess.StringNumber=-1;
            ess.StringEOL=egs.StringEOL;
            ess.StringLength=egs.StringLength+CoordX;
            if(ei.CurPos>=egs.StringLength)
              ess.StringLength+=ei.CurPos-egs.StringLength;
            ess.StringText=static_cast<wchar_t*>(malloc(ess.StringLength*sizeof(wchar_t)));
            if(ess.StringText)
            {
              if(egs.StringLength>ei.CurPos)
                 memcpy((wchar_t *)ess.StringText, egs.StringText, ei.CurPos*sizeof(wchar_t));
              else
              {
                 memcpy((wchar_t *)ess.StringText, egs.StringText, egs.StringLength*sizeof(wchar_t));
                 wwmemset((wchar_t *)ess.StringText+egs.StringLength, nlsSpace,
                         ei.CurPos-egs.StringLength);
              }
              wwmemset((wchar_t *)ess.StringText+ei.CurPos, nlsSpace, CoordX);
              if(egs.StringLength>ei.CurPos)
                 memcpy((wchar_t *)ess.StringText+ei.CurPos+CoordX,
                        egs.StringText+ei.CurPos, (egs.StringLength-ei.CurPos)*sizeof(wchar_t));
              if(EditorControl(ECTL_SETSTRING,&ess))
              {
                InitESPandEGS(esp, egs);
                esp.CurPos=ei.CurPos+CoordX;
                EditorControl(ECTL_SETPOSITION, &esp);
                EditorControl(ECTL_REDRAW, NULL);
                RetCode=1;
              }
              free((wchar_t *)ess.StringText);
            }
          }
        }
      }
      if(RetCode)
        EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
      isReenter=0;
      _D(SysLog(L"PEI: 10 return"));
      return RetCode;
    }

    if(IsLocked)
    {
      _D(SysLog(L"PEI: 11 return"));
      return 0;
    }

    // wVirtualKeyCode, ��ࠡ�⪠ ������ ࠧ�襭� ⮫쪮 �� !IsLocked
    if(KeyEvent.wVirtualKeyCode==VK_END)
    {
      if(nodedata.Options&E_ProcessKeyEnd_On)
      {
        isReenter=1;
        InitNLS(ei, nodedata);
        InitESPandEGS(esp, egs);
        prc_Minuses=nodedata.Options&E_Process_Minuses_On;
        KillSpaces();
        isReenter=0;
      }
      _D(SysLog(L"PEI: 12 return"));
      return 0;
    }
    else if(KeyEvent.wVirtualKeyCode==VK_BACK)
    {
      RetCode=0;
      if(nodedata.Lines && (nodedata.Options&E_SmartBackSpace_On) &&
         (ei.Options&(EOPT_EXPANDALLTABS|EOPT_EXPANDONLYNEWTABS)) &&
         ei.CurPos && !(KeyEvent.dwControlKeyState & SHIFT_PRESSED) &&
         (ei.BlockType==BTYPE_NONE || !(ei.Options&EOPT_DELREMOVESBLOCKS) ||
          ei.Options&EOPT_PERSISTENTBLOCKS)
        )
      {
        Lines=nodedata.Lines;
        isReenter=1;
        InitESPandEGS(esp, egs);
        InitNLS(ei, nodedata);
        EditorControl(ECTL_GETSTRING, &egs);
        int X=Min(egs.StringLength, ei.CurPos);
        for(int f=X-1;f>=0;--f)
        {
          if(!IsCSpaceOrTab(egs.StringText[f]))
          {
             X=-1;
             break;
          }
        }

        CoordX=(X==-1)?-1:GetPrevCoordX(ei, Lines, nlsStopChars->str);
        //_D(SysLog(L"SmartBackSpace: %d", CoordX));
        if(CoordX>-1)
        {
          EditorControl(ECTL_GETSTRING, &egs);
          ess.StringNumber=-1;
          ess.StringEOL=egs.StringEOL;
          ess.StringLength=egs.StringLength+CoordX;
          if(ei.CurPos>=egs.StringLength)
            ess.StringLength=egs.StringLength?CoordX:0;
          else
            ess.StringLength=egs.StringLength-(ei.CurPos-CoordX);
          ess.StringText=static_cast<wchar_t*>(malloc((ess.StringLength+1)*sizeof(wchar_t)));
          if(ess.StringText)
          {
            if(egs.StringLength>CoordX)
              memcpy((wchar_t *)ess.StringText, egs.StringText, CoordX*sizeof(wchar_t));
            else
              memcpy((wchar_t *)ess.StringText, egs.StringText, egs.StringLength*sizeof(wchar_t));

            if(egs.StringLength>ei.CurPos)
              memcpy((wchar_t *)ess.StringText+CoordX,
                     egs.StringText+ei.CurPos, (egs.StringLength-ei.CurPos)*sizeof(wchar_t));
            if(EditorControl(ECTL_SETSTRING,&ess))
            {
              InitESPandEGS(esp, egs);
              esp.CurPos=CoordX;
              EditorControl(ECTL_SETPOSITION, &esp);
              EditorControl(ECTL_REDRAW, NULL);
              RetCode=1;
              EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
            }
            free((wchar_t *)ess.StringText);
          }
        }
        isReenter=0;
      }
      _D(SysLog(L"PEI: 13 return"));
      return RetCode;
    }
    else if(KeyEvent.wVirtualKeyCode==VK_RETURN)
    {
      RetCode=0;
      if(isWrapQuote &&
         !(!(ei.Options&EOPT_PERSISTENTBLOCKS) && ei.BlockType!=BTYPE_NONE) &&
         !(KeyEvent.dwControlKeyState & SHIFT_PRESSED)
        )
      {
        InitESPandEGS(esp, egs);
        InitNLS(ei, nodedata);
        EditorControl(ECTL_GETSTRING,&egs);
        RetCode=ProcessKeyEnter(ei, esp, egs,
                IsQuote(egs.StringText,egs.StringLength),
                nodedata.Options&E_QuoteEOL_On);
        if(RetCode)
          EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
      }
      _D(SysLog(L"PEI: 14 return"));
      return RetCode;
    }
    else if(KeyEvent.wVirtualKeyCode==VK_DELETE)
    {
      RetCode=0;
      if(isWrapQuote &&
         !(ei.Options&EOPT_DELREMOVESBLOCKS && ei.BlockType!=BTYPE_NONE) &&
         !(KeyEvent.dwControlKeyState & SHIFT_PRESSED)
        )
      {
        InitESPandEGS(esp, egs);
        InitNLS(ei, nodedata);
        EditorControl(ECTL_GETSTRING,&egs);
        if(ei.CurPos>=egs.StringLength)
        {
          RetCode=ProcessKeyDelete(egs,
                  IsQuote(egs.StringText,egs.StringLength));
          if(RetCode)
            EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
        }
      }
      _D(SysLog(L"PEI: 15 return"));
      return RetCode;
    }
    else if(nodedata.Options&E_AddSymbol_On)
    {
      isReenter=1;
      if(ei.BlockType!=BTYPE_NONE && !(ei.Options&EOPT_PERSISTENTBLOCKS))
      {
        EditorControl(ECTL_DELETEBLOCK,NULL);
        EditorControl(ECTL_GETINFO,&ei);
      }
      RetCode=0;
      InitNLS(ei, nodedata);
      //_D(SysLog(L"AsciiChar=[%c] AddSym_S=[%s]",KeyEvent.uChar.AsciiChar, nodedata.AddSym_S.str));
      if(nodedata.AddSym_S.getLength() &&
         nodedata.AddSym_S.getLength()==nodedata.AddSym_E.getLength())
         RetCode=InsertAdditionalSymbol(ei, esp, ess, egs,
                            nodedata.AddSym_S, nodedata.AddSym_E,
                            KeyEvent.uChar.AsciiChar, TRUE);
      if(!RetCode &&
         nodedata.AddSym_S_B.getLength() &&
         nodedata.AddSym_S_B.getLength()==nodedata.AddSym_E_B.getLength())
         RetCode=InsertAdditionalSymbol(ei, esp, ess, egs,
                            nodedata.AddSym_S_B, nodedata.AddSym_E_B,
                            KeyEvent.uChar.AsciiChar, FALSE);
      isReenter=0;
      if(RetCode)
      {
        EditorControl(ECTL_TURNOFFMARKINGBLOCK,NULL);
        _D(SysLog(L"PEI: 16 return"));
        return TRUE;
      }
    }

    struct EditorConvertText ect, ect2;
    wchar_t* buff, *buff1, *buff2=NULL, *buff3=NULL;
    int Blank;
    int i;
    int nbCount, nbExtra;
    int nWrapPos=CalcWrapPos(nodedata, ei),
        isJustifyEnabled=(nodedata.Options2&E_Wrap_Justify)?1:0;
    div_t SpaceCount;

    _D(SysLog(L"PEI: Wrap=%d", !!(nodedata.Options&E_AutoWrap_On)));
    if( nWrapPos<1 || !(nodedata.Options&E_AutoWrap_On))
    {
      _D(SysLog(L"PEI: 17 return"));
      return 0;
    }

    if( ei.CurTabPos>=nWrapPos ){

        isReenter=1;
        EditorControl(ECTL_PROCESSINPUT,(void*)Rec);
        isReenter=0;

        i=-1;
        EditorControl(ECTL_EXPANDTABS,&i); // probably it's needed to pack it back later

        EditorControl(ECTL_GETINFO, &ei);
        InitNLS(ei, nodedata);

        InitESPandEGS(esp, egs);
        EditorControl(ECTL_GETSTRING,&egs);

        _D(SysLog(L"PEI: ei.CurTabPos=%d, nWrapPos=%d, egs.StringLength=%d",
           ei.CurPos, nWrapPos, egs.StringLength));
        if( ei.CurTabPos>nWrapPos && ei.CurTabPos==egs.StringLength ){ // must be at the end of string
          ect.Text=buff1=(wchar_t*)malloc(egs.StringLength*sizeof(wchar_t));
          buff=(wchar_t*)malloc(nWrapPos*sizeof(wchar_t));
          if(buff1 && buff){
            memcpy(ect.Text,egs.StringText,egs.StringLength*sizeof(wchar_t));
            ect.TextLength=egs.StringLength;

            memcpy(buff,ect.Text,nWrapPos*sizeof(wchar_t));

            Blank=0;
            if( ect.TextLength>nWrapPos && ect.Text[nWrapPos]==nlsSpace )
                Blank=1;
            for( ess.StringLength=nWrapPos-1; ess.StringLength>=0; ess.StringLength-- ){
                if( !Blank ){
                    if( buff[ess.StringLength]==nlsSpace )Blank=1;
                }else{
                    if( buff[ess.StringLength]!=nlsSpace ){
                        ess.StringLength++;
                        break;
                    }
                }
            }

            int nQuote=isWrapQuote?IsQuote(buff,ect.TextLength):0;

            if( ess.StringLength>0 ){ // we have found the blank where to crop the string...

                // portion of string that will go to the next line
                ect.TextLength-=ess.StringLength;
                ect.Text+=ess.StringLength;

                // skip leading blanks in the rest of the string
                for( ; ect.TextLength && ect.Text[0]==nlsSpace; ect.TextLength-- )
                    ect.Text++;

                if( isJustifyEnabled ){
                    // make justify on buff...

                    ect2.Text=buff2=(wchar_t*)malloc(nWrapPos*sizeof(wchar_t));
                    if(!buff2) goto EXIT;
                    ect2.TextLength=0;

                    while( ess.StringLength ){ // trim end spaces
                        if( buff[ess.StringLength-1]==nlsSpace ) ess.StringLength--;
                        else break;
                    }
                    for( i=0; i<ess.StringLength; i++ ){
                        if( buff[i]!=nlsSpace ) break; // skip leading blanks
                        else ect2.Text[ect2.TextLength++]=nlsSpace;
                    }
                    for( nbCount=Blank=0; i<ess.StringLength; i++ ){
                        if( Blank<=0 ){ // it was also blank before or just beggining
                            if( buff[i]!=nlsSpace ){
                                Blank=-Blank+1; // increment word count
                                nbCount++;
                            }
                        }else{ // it was a word before
                            if( buff[i]==nlsSpace ) Blank=-Blank;
                            else nbCount++;
                        }
                    }
                    if( Blank<0 ) Blank=-Blank;
                    // now in Blank we have word count and non-blank syms count in nbCount...

                    // now let us try to get some words from the resting string portion...
                    if( ect.TextLength ){ // if something really rested
                        nbExtra=0;
                        for( i=0; i<=ect.TextLength; i++ ){
                            if( i==ect.TextLength || ect.Text[i]==nlsSpace ){
                                if( ect2.TextLength+Blank+nbCount /* the most comact string len+1 */
                                    + nbExtra > nWrapPos ) break;
                                Blank++;
                                nbCount+=nbExtra;
                                nbExtra=0;
                                while( ++i<ect.TextLength ){
                                    if( ect.Text[i]!=nlsSpace ){
                                        i--;
                                        break;
                                    }
                                }
                            }else
                                nbExtra++;
                        }
                    }

                    if( Blank==1 ){

                        ect2.Text=buff;
                        ect2.TextLength=ess.StringLength;

                    }else{

                        SpaceCount=div(nWrapPos-nbCount-ect2.TextLength,--Blank);
                        SpaceCount.rem=Blank-SpaceCount.rem;

                        // now we have minimum space length in SpaceCount.quot
                        // and amount of blank fields with _no_ extra spacing in SpaceCount.rem

                        for( i=ect2.TextLength; i<ess.StringLength; ){
                            if( buff[i]==nlsSpace ){
                                wwmemset( ect2.Text+ect2.TextLength, nlsSpace, SpaceCount.quot );
                                ect2.TextLength+=SpaceCount.quot;
                                if( SpaceCount.rem ) SpaceCount.rem--;
                                else ect2.Text[ect2.TextLength++]=nlsSpace;
                                Blank--;
                                while( i++ < ess.StringLength )
                                    if( buff[i]!=nlsSpace ) break;
                            }else
                                ect2.Text[ect2.TextLength++]=buff[i++];
                        }

                        // fill up to nWrapPos with the initial part of the resting string...
                        if( Blank ){
                            wwmemset( ect2.Text+ect2.TextLength, nlsSpace, SpaceCount.quot);
                            ect2.TextLength+=SpaceCount.quot;
                            if( SpaceCount.rem ) SpaceCount.rem--;
                            else ect2.Text[ect2.TextLength++]=nlsSpace;
                            Blank--;
                            for( i=0; i<ect.TextLength; ){
                                if( ect.Text[i]==nlsSpace ){
                                    if( Blank ){
                                        wwmemset( ect2.Text+ect2.TextLength, nlsSpace, SpaceCount.quot );
                                        ect2.TextLength+=SpaceCount.quot;
                                        if( SpaceCount.rem ) SpaceCount.rem--;
                                        else ect2.Text[ect2.TextLength++]=nlsSpace;
                                    }
                                    while( i < ect.TextLength )
                                        if( ect.Text[i]!=nlsSpace ) break;
                                        else i++;
                                    if( Blank-- == 0 ) break;
                                }else
                                    ect2.Text[ect2.TextLength++]=ect.Text[i++];
                            }
                            ect.Text+=i;
                            ect.TextLength-=i;
                        }
                    }

                }else{
                    ect2.Text=buff;
                    ect2.TextLength=ess.StringLength;
                }

                ess.StringText=ect2.Text;
                ess.StringLength=ect2.TextLength;
                ess.StringNumber=-1;
                ess.StringEOL=const_cast<wchar_t*>(egs.StringEOL);
                EditorControl(ECTL_SETSTRING,&ess);

                // may be here we need to pack the tabs back, if tabs are enabled...
                // this time for the current line we will forget now forever...

                if( ect.TextLength || KeyEvent.wVirtualKeyCode==VK_SPACE || KeyEvent.wVirtualKeyCode==VK_TAB ){
                    EditorControl(ECTL_INSERTSTRING,0);

                    if( (ei.Options&EOPT_AUTOINDENT) && nQuote==0 ){

                        for( i=0; i<ess.StringLength; i++ )
                            if( ess.StringText[i]!=nlsSpace ) break;

                        ess.StringLength=ect.TextLength+i;
                        ess.StringText=buff3=(wchar_t*)malloc((ess.StringLength+1)*sizeof(wchar_t));
                        if( ess.StringText==NULL ){
                            ess.StringLength=ect.TextLength;
                            ess.StringText=ect.Text;
                        }else{
                            wwmemset((wchar_t *)ess.StringText,nlsSpace,i);
                            memcpy((wchar_t *)ess.StringText+i,ect.Text,ect.TextLength*sizeof(wchar_t));
                        }
                    }else{
                        ess.StringLength=ect.TextLength;
                        ess.StringText=ect.Text;
                    }

                    EditorControl(ECTL_SETSTRING,&ess);

                    if( nQuote ){ // insert wrapped quote from pervious line...
                        esp.CurLine=-1;
                        esp.CurPos=0;
                        esp.CurTabPos=-1;
                        esp.TopScreenLine=-1;
                        esp.LeftPos=-1;
                        esp.Overtype=-1;
                        EditorControl(ECTL_SETPOSITION,&esp);

                        buff[nQuote]=L'\0';
                        ect.Text=buff;
                        ect.TextLength=nQuote;
                        EditorControl(ECTL_EDITORTOOEM,&ect);
                        EditorControl(ECTL_INSERTTEXT,buff);
                    }

                    // may be here we need to pack the tabs, if tabs are enabled...
                }else nQuote=0; // we do nothing with quote this time...

                esp.CurLine=-1;
                esp.CurPos=ess.StringLength+nQuote;
                esp.CurTabPos=-1;
                esp.TopScreenLine=-1;
                esp.LeftPos=-1;
                esp.Overtype=-1;
                EditorControl(ECTL_SETPOSITION,&esp);

                EditorControl(ECTL_REDRAW,NULL);
            }
          }
EXIT:
          if(buff3) free(buff3);
          if(buff2) free(buff2);
          if(buff)  free(buff);
          if(buff1) free(buff1);
        }
        // may be here we need to pack the tabs back, if tabs are enabled...
        _D(SysLog(L"PEI: 18 return"));
        return 1;
    }
    _D(SysLog(L"PEI: last return"));
    return 0;
}

#ifdef __cplusplus
extern "C"
{
#endif
int WINAPI _export
GetEditorSettingsW(int EditorID, const wchar_t *szName, void *Param)
{
  if(!szName || !Param || *szName==0 || !NodeData || !ESETStorage)
    return FALSE;

  EditorSettingsStorage ESS(EditorID);
  Node_<EditorSettingsStorage> *Item=ESETStorage->findNode(ESS);
  if(NULL==Item) return FALSE;

  const NODEDATA &Data=Item->data.Data;
  BOOL RetCode=TRUE;

  if(!wstrcmp(szName, XMLStr.TabSize))
  {
     *static_cast<int *>(Param)=Data.TabSize;
  }
  else if(!wstrcmp(szName, XMLStr.ExpandTabs))
  {
     if(Data.Options&E_ExpandTabs_On)
       *static_cast<int *>(Param)=1;
     else if(Data.Options&E_ExpandTabs_OnlyNew)
       *static_cast<int *>(Param)=3;
     else if(Data.Options&E_ExpandTabs_Off)
       *static_cast<int *>(Param)=0;
     else
       *static_cast<int *>(Param)=2;
  }
  else if(!wstrcmp(szName, XMLStr.CursorBEOL))
  {
     *static_cast<int *>(Param)=(Data.Options&E_CursorBeyondEOL_On)?1:
       ((Data.Options&E_CursorBeyondEOL_Off)?0:2);
  }
  else if(!wstrcmp(szName, XMLStr.AutoIndent))
  {
     *static_cast<int *>(Param)=(Data.Options&E_AutoIndent_On)?1:
       ((Data.Options&E_AutoIndent_On)?0:2);
  }
  else if(!wstrcmp(szName, XMLStr.CharCodeBase))
  {
     *static_cast<int *>(Param)=(Data.Options&E_CharCodeBase_Oct)?8:
       ((Data.Options&E_CharCodeBase_Dec)?10:
        ((Data.Options&E_CharCodeBase_Hex)?16:0));
  }
  else if(!wstrcmp(szName, XMLStr.KillSpaces))
  {
     *static_cast<int *>(Param)=(Data.Options&E_KillSpaces_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.p_Minuses))
  {
     *static_cast<int *>(Param)=(Data.Options&E_Process_Minuses_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.KillEmptyLines))
  {
     *static_cast<int *>(Param)=(Data.Options&E_KillEmptyLines_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.ForceKillEmptyLines))
  {
     *static_cast<int *>(Param)=(Data.Options&E_ForceKillEL_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.AutoWrap))
  {
     *static_cast<int *>(Param)=(Data.Options&E_AutoWrap_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.Wrap))
  {
     EditorControl(ECTL_GETINFO, &ei);
     *static_cast<int *>(Param)=CalcWrapPos(Data, ei);
  }
  else if(!wstrcmp(szName, XMLStr.Justify))
  {
     *static_cast<int *>(Param)=(Data.Options2&E_Wrap_Justify)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.eol))
  {
     *static_cast<DWORD *>(Param)=Data.EOL;
  }
  else if(!wstrcmp(szName, XMLStr.Table))
  {
     *static_cast<DWORD *>(Param)=Data.Table;
  }
  else if(!wstrcmp(szName, XMLStr.ProcessKeyEnd))
  {
     *static_cast<int *>(Param)=(Data.Options&E_ProcessKeyEnd_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.SmartTab))
  {
     *static_cast<int *>(Param)=(Data.Options&E_SmartTab_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.SmartBackSpace))
  {
     *static_cast<int *>(Param)=(Data.Options&E_SmartBackSpace_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.Lines))
  {
     *static_cast<int *>(Param)=Data.Lines;
  }
  else if(!wstrcmp(szName, XMLStr.SmartHome))
  {
     *static_cast<int *>(Param)=(Data.Options&E_SmartHome_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.ProcessQuote))
  {
     *static_cast<int *>(Param)=(Data.Options&E_ProcessQuote_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.QuoteEOL))
  {
     *static_cast<int *>(Param)=(Data.Options&E_QuoteEOL_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.SaveFilePos))
  {
     *static_cast<int *>(Param)=(Data.Options&E_SaveFilePos_On)?1:
       ((Data.Options&E_SaveFilePos_Off)?0:2);
  }
  else if(!wstrcmp(szName, XMLStr.LockMode))
  {
     *static_cast<int *>(Param)=(Data.Options&E_LockMode_On)?1:
       ((Data.Options&E_LockMode_Off)?0:2);
  }
  else if(!wstrcmp(szName, XMLStr.MinLinesNum))
  {
     *static_cast<int *>(Param)=Data.MinLinesNum;
  }
  else if(!wstrcmp(szName, XMLStr.AddSymbol))
  {
    *static_cast<int *>(Param)=(Data.Options&E_AddSymbol_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.LockFile))
  {
    *static_cast<int *>(Param)=Data.LockFile.IsOn()?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.WordSym))
  {
    *static_cast<int *>(Param)=(Data.Options2&E_WordSym_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.AlphaNum))
  {
    *static_cast<int *>(Param)=(Data.Options2&E_AlphaNum_On)?1:0;
  }
  else if(!wstrcmp(szName, XMLStr.Additional) && Param!=NULL)
  {
    wstrcpy(static_cast<wchar_t *>(Param), Data.AdditionalLetters.str);
  }
  else
    RetCode=FALSE;

  return RetCode;
}

int WINAPI _export
SetEditorOptionW(int EditorID, const wchar_t *szName, void *Param)
{
  if(!szName || !Param || *szName==0 || !NodeData || !ESETStorage)
    return FALSE;

  EditorSettingsStorage ESS(EditorID);
  Node_<EditorSettingsStorage> *Item=ESETStorage->findNode(ESS);
  if(NULL==Item) return FALSE;

  NODEDATA &Data=Item->data.Data;
  const NODEDATA oldData(Data);
  int RetCode=TRUE, RCisFALSEbutApplyIs=FALSE, param=*static_cast<int *>(Param);
  DWORD Type(-1); //EDITOR_SETPARAMETER_TYPES

  if(!wstrcmp(szName, XMLStr.TabSize))
  {
    Data.TabSize=param;
    Type=ESPT_TABSIZE;
  }
  else if(!wstrcmp(szName, XMLStr.ExpandTabs))
  {
    Data.Options&=~E_ExpandTabs_On;
    Data.Options&=~E_ExpandTabs_Off;
    Data.Options&=~E_ExpandTabs_OnlyNew;
    if(1==param)
      Data.Options|=E_ExpandTabs_On;
    else if(0==param)
      Data.Options|=E_ExpandTabs_Off;
    else if(3==param)
      Data.Options|=E_ExpandTabs_OnlyNew;
    Type=ESPT_EXPANDTABS;
  }
  else if(!wstrcmp(szName, XMLStr.CursorBEOL))
  {
    Data.Options&=~E_CursorBeyondEOL_On;
    Data.Options&=~E_CursorBeyondEOL_Off;
    Data.Options|=(param==1)?E_CursorBeyondEOL_On:
                  ((param==0)?E_CursorBeyondEOL_Off:0);
    Type=ESPT_CURSORBEYONDEOL;
  }
  else if(!wstrcmp(szName, XMLStr.AutoIndent))
  {
    Data.Options&=~E_AutoIndent_On;
    Data.Options&=~E_AutoIndent_Off;
    Data.Options|=(param==1)?E_AutoIndent_On:
                  ((param==0)?E_AutoIndent_Off:0);
    Type=ESPT_AUTOINDENT;
  }
  else if(!wstrcmp(szName, XMLStr.CharCodeBase))
  {
    Data.Options&=~E_CharCodeBase_Dec;
    Data.Options&=~E_CharCodeBase_Oct;
    Data.Options&=~E_CharCodeBase_Hex;
    Data.Options|=(param==10)?E_CharCodeBase_Dec:
                   ((param==8)?E_CharCodeBase_Oct:
                    ((param==16)?E_CharCodeBase_Hex:0)
                   );
    Type=ESPT_CHARCODEBASE;
  }
  else if(!wstrcmp(szName, XMLStr.KillSpaces))
  {
    if(param)
      Data.Options|=E_KillSpaces_On;
    else
    {
      Data.Options&=~E_KillSpaces_On;
      Data.Options&=~E_ProcessKeyEnd_On;
    }
  }
  else if(!wstrcmp(szName, XMLStr.p_Minuses))
  {
    if(param) Data.Options|=E_Process_Minuses_On;
    else      Data.Options&=~E_Process_Minuses_On;
  }
  else if(!wstrcmp(szName, XMLStr.KillEmptyLines))
  {
    if(param) Data.Options|=E_KillEmptyLines_On;
    else      Data.Options&=~E_KillEmptyLines_On;
  }
  else if(!wstrcmp(szName, XMLStr.ForceKillEmptyLines))
  {
    if(param) Data.Options|=E_ForceKillEL_On;
    else      Data.Options&=~E_ForceKillEL_On;
  }
  else if(!wstrcmp(szName, XMLStr.AutoWrap))
  {
    if(param) Data.Options|=E_AutoWrap_On;
    else      Data.Options&=~E_AutoWrap_On;
  }
  else if(!wstrcmp(szName, XMLStr.Wrap))
  {
    Data.Options2&=~E_Wrap_Percent;
    Data.WrapPos=param;
  }
  else if(!wstrcmp(szName, XMLStr.Justify))
  {
    if(param) Data.Options2|=E_Wrap_Justify;
    else      Data.Options2&=~E_Wrap_Justify;
  }
  else if(!wstrcmp(szName, XMLStr.eol))
  {
    Data.EOL=*static_cast<DWORD*>(Param);
  }
  else if(!wstrcmp(szName, XMLStr.Table))
  {
    Data.Table=*static_cast<DWORD *>(Param);
  }
  else if(!wstrcmp(szName, XMLStr.ProcessKeyEnd))
  {
    if(param)
    {
      RCisFALSEbutApplyIs=TRUE;
      Data.Options&=~E_ProcessKeyEnd_On;
      if(Data.Options&E_KillSpaces_On)
      {
        Data.Options|=E_ProcessKeyEnd_On;
        RetCode=TRUE;
      }
      else
        RetCode=FALSE;
    }
    else
      Data.Options&=~E_ProcessKeyEnd_On;
  }
  else if(!wstrcmp(szName, XMLStr.SmartTab))
  {
    if(param) Data.Options|=E_SmartTab_On;
    else      Data.Options&=~E_SmartTab_On;
  }
  else if(!wstrcmp(szName, XMLStr.SmartBackSpace))
  {
    if(param) Data.Options|=E_SmartBackSpace_On;
    else      Data.Options&=~E_SmartBackSpace_On;
  }
  else if(!wstrcmp(szName, XMLStr.Lines))
  {
    Data.Lines=param;
  }
  else if(!wstrcmp(szName, XMLStr.SmartHome))
  {
    if(param) Data.Options|=E_SmartHome_On;
    else      Data.Options&=~E_SmartHome_On;
  }
  else if(!wstrcmp(szName, XMLStr.ProcessQuote))
  {
    if(param) Data.Options|=E_ProcessQuote_On;
    else      Data.Options&=~E_ProcessQuote_On;
  }
  else if(!wstrcmp(szName, XMLStr.QuoteEOL))
  {
    if(param) Data.Options|=E_QuoteEOL_On;
    else      Data.Options&=~E_QuoteEOL_On;
  }
  else if(!wstrcmp(szName, XMLStr.SaveFilePos))
  {
    Data.Options&=~E_SaveFilePos_On;
    Data.Options&=~E_SaveFilePos_Off;
    Data.Options|=(param==1)?E_SaveFilePos_On:
                  ((param==0)?E_SaveFilePos_Off:0);
    Type=ESPT_SAVEFILEPOSITION;
  }
  else if(!wstrcmp(szName, XMLStr.LockMode))
  {
    Data.Options&=~E_LockMode_On;
    Data.Options&=~E_LockMode_Off;
    Data.Options|=(param==1)?E_LockMode_On:
                  ((param==0)?E_LockMode_Off:0);
    Type=ESPT_LOCKMODE;
  }
  else if(!wstrcmp(szName, XMLStr.MinLinesNum))
  {
    Data.MinLinesNum=param;
    Type=ESPT_SAVEFILEPOSITION;
  }
  else if(!wstrcmp(szName, XMLStr.AddSymbol))
  {
    if(param) Data.Options|=E_AddSymbol_On;
    else      Data.Options&=~E_AddSymbol_On;
  }
  else if(!wstrcmp(szName, XMLStr.LockFile))
  {
    if(param)
    {
      Data.Options|=E_LockFile_On;
      Data.LockFile.On();
    }
    else
    {
      Data.Options&=~E_LockFile_On;
      Data.LockFile.Off();
    }
  }
  else if(!wstrcmp(szName, XMLStr.WordSym))
  {
    if(param) Data.Options2|=E_WordSym_On;
    else      Data.Options2&=~E_WordSym_On;
    Type=ESPT_SETWORDDIV;
  }
  else if(!wstrcmp(szName, XMLStr.AlphaNum))
  {
    if(param) Data.Options2|=E_AlphaNum_On;
    else      Data.Options2&=~E_AlphaNum_On;
    Type=ESPT_SETWORDDIV;
  }
  else if(!wstrcmp(szName, XMLStr.Additional))
  {
    Data.AdditionalLetters=Param?static_cast<wchar_t*>(Param):L"";
    Type=ESPT_SETWORDDIV;
  }
  else
    RetCode=FALSE;

  if(RetCode && Type!=static_cast<DWORD>(-1))
  {
    CEditorOptions eo(Data);
    RetCode=eo.ApplyOption(static_cast<EDITOR_SETPARAMETER_TYPES>(Type));
  }

  if(RetCode || RCisFALSEbutApplyIs)
    Item->data.index=-1;
  else
    Item->data.Data=oldData;

  return RetCode;
}

BOOL WINAPI _export CreateMultiMacroW(HANDLE *Macro, const wchar_t *Sequence, DWORD total)
{
  if(Macro)
  {
    if(!*Macro)
      *Macro=new KeySequenceStorage();
    if(*Macro)
    {
      int Error;
      strcon s;
      return static_cast<KeySequenceStorage*>(*Macro)->compile(Sequence,true,
        total,TRUE,Error,s);
    }
  }
  return FALSE;
}

BOOL WINAPI _export CreateMacroW(HANDLE *Macro, const wchar_t *Sequence)
{
  return CreateMultiMacroW(Macro,Sequence,1);
}

BOOL WINAPI _export CloneMacroW(const HANDLE Src, HANDLE *Dest)
{
  if(Dest && Src)
  {
    if(!*Dest)
      *Dest=new KeySequenceStorage();
    if(*Dest)
    {
      *static_cast<KeySequenceStorage*>(*Dest)=
        *static_cast<const KeySequenceStorage*>(Src);
      return TRUE;
    }
  }
  return FALSE;
}

BOOL WINAPI _export IsMacroOKW(const HANDLE Macro)
{
  return Macro?
    static_cast<const KeySequenceStorage*>(Macro)->IsOK():FALSE;
}

void WINAPI _export DeleteMacroW(HANDLE *Macro)
{
  if(Macro)
  {
     delete static_cast<KeySequenceStorage*>(*Macro);
     *Macro=NULL;
  }
}

BOOL WINAPI _export PostMacroW(const HANDLE Macro, BOOL silent)
{
   const KeySequenceStorage *Cmd=static_cast<const KeySequenceStorage*>(Macro);
   if(Cmd && Cmd->IsOK())
   {
      BOOL Stop;
      KeySequence KS=Cmd->GetSequence(Stop);
      KS.Flags=silent?KSFLAGS_DISABLEOUTPUT:0;
      return FARPostMacro(&KS);
   }
   return FALSE;
}

#ifdef __cplusplus
}
#endif