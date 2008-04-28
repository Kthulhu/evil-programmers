/*
    um_dialogs2.cpp
    Copyright (C) 2001-2007 zg

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include "..\..\plugin.hpp"
#include <lm.h>
#include "umplugin.h"
#include "memory.h"

long WINAPI EditShareDialogProc(HANDLE hDlg,int Msg,int Param1,long Param2)
{
  FarDialogItem DialogItem;
  switch(Msg)
  {
    case DN_INITDIALOG:
      Info.SendDlgMessage(hDlg,DM_SETTEXTLENGTH,4,MAX_COMMENT-1);
    case DN_BTNCLICK:
      Info.SendDlgMessage(hDlg,DM_GETDLGITEM,6,(long)&DialogItem);
      if(DialogItem.Selected)
      {
        for(int i=8;i<10;i++)
        {
          Info.SendDlgMessage(hDlg,DM_GETDLGITEM,i,(long)&DialogItem);
          DialogItem.Flags|=DIF_DISABLE;
          Info.SendDlgMessage(hDlg,DM_SETDLGITEM,i,(long)&DialogItem);
        }
      }
      else
      {
        for(int i=8;i<10;i++)
        {
          Info.SendDlgMessage(hDlg,DM_GETDLGITEM,i,(long)&DialogItem);
          DialogItem.Flags&=~DIF_DISABLE;
          Info.SendDlgMessage(hDlg,DM_SETDLGITEM,i,(long)&DialogItem);
        }
      }
      break;
  }
  return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}

bool EditShareProperties(UserManager *panel)
{
  bool res=false;
  PanelInfo PInfo;
  Info.Control((HANDLE)panel,FCTL_GETPANELINFO,&PInfo);
  if((PInfo.ItemsNumber>0)&&(strcmp("..",PInfo.PanelItems[PInfo.CurrentItem].FindData.cFileName)))
  {
    if(PInfo.PanelItems[PInfo.CurrentItem].Flags&PPIF_USERDATA)
    {
      SHARE_INFO_502 *info;
      if(NetShareGetInfo(panel->computer_ptr,GetWideNameFromUserData(PInfo.PanelItems[PInfo.CurrentItem].UserData),502,(LPBYTE *)&info)==NERR_Success)
      {
        //Show dialog
        /*
          0000000000111111111122222222223333333333444444444455555555556666666666777777
          0123456789012345678901234567890123456789012345678901234567890123456789012345
        00                                                                            00
        01   ����������������������������� Edit Share ���������������������������ͻ   01
        02   � Share Name:                                                        �   02
        03   � TEMP                                                              |�   03
        04   � Comment:                                                           �   04
        05   � Temp dir                                                          |�   05
        06   � �� User Limit ��������������������������������������������������Ŀ �   06
        07   � � ( ) Maximum allowed                                            � �   07
        08   � � ( ) Allow                                                      � �   08
        09   � �     999999999 Users                                            � �   09
        10   � ������������������������������������������������������������������ �   10
        11   ��������������������������������������������������������������������Ķ   11
        12   �                 [ OK ]                  [ Cancel ]                 �   12
        13   ��������������������������������������������������������������������ͼ   13
        14                                                                            14
          0000000000111111111122222222223333333333444444444455555555556666666666777777
          0123456789012345678901234567890123456789012345678901234567890123456789012345
        */
        static char *CommentShareHistoryName="UserManager\\CommentShare";
        static struct InitDialogItem InitDlg[]={
        /* 0*/  {DI_DOUBLEBOX,3,1,72,13,0,0,0,0,(char *)mShareEditShare},
        /* 1*/  {DI_TEXT,5,2,0,0,0,0,0,0,(char *)mShareShareName},
        /* 2*/  {DI_TEXT,5,3,0,0,0,0,DIF_SHOWAMPERSAND,0,""},
        /* 3*/  {DI_TEXT,5,4,0,0,0,0,0,0,(char *)mShareComment},
        /* 4*/  {DI_EDIT,5,5,70,0,1,(DWORD)CommentShareHistoryName,DIF_HISTORY,0,""},
        /* 5*/  {DI_SINGLEBOX,5,6,70,10,0,0,DIF_LEFTTEXT,0,(char *)mShareUserLimit},
        /* 6*/  {DI_RADIOBUTTON,7,7,0,0,0,0,DIF_GROUP,0,(char *)mShareMaximumAllowed},
        /* 7*/  {DI_RADIOBUTTON,7,8,0,0,0,0,0,0,(char *)mShareAllow},
        /* 8*/  {DI_FIXEDIT,11,9,19,0,0,(int)"########9",DIF_MASKEDIT,0,""},
        /* 9*/  {DI_TEXT,21,9,0,0,0,0,0,0,(char *)mShareUsers},
        /*10*/  {DI_TEXT,5,11,0,0,0,0,DIF_BOXCOLOR|DIF_SEPARATOR,0,""},
        /*11*/  {DI_BUTTON,0,12,0,0,0,0,DIF_CENTERGROUP,1,(char *)mButtonOk},
        /*12*/  {DI_BUTTON,0,12,0,0,0,0,DIF_CENTERGROUP,0,(char *)mButtonCancel},
        };
        struct FarDialogItem DialogItems[sizeof(InitDlg)/sizeof(InitDlg[0])];
        InitDialogItems(InitDlg,DialogItems,sizeof(InitDlg)/sizeof(InitDlg[0]));
        WideCharToMultiByte(CP_OEMCP,0,(wchar_t *)info->shi502_netname,-1,DialogItems[2].Data,MAX_PATH,NULL,NULL);
        WideCharToMultiByte(CP_OEMCP,0,(wchar_t *)info->shi502_remark,-1,DialogItems[4].Data,MAX_COMMENT,NULL,NULL);
        if(info->shi502_max_uses==0xffffffff)
        {
          DialogItems[6].Selected=TRUE;
          FSF.itoa(0,DialogItems[8].Data,10);
        }
        else
        {
          DialogItems[7].Selected=TRUE;
          FSF.itoa(info->shi502_max_uses,DialogItems[8].Data,10);
        }
        int DlgCode=Info.DialogEx(Info.ModuleNumber,-1,-1,76,15,"EditShare",DialogItems,(sizeof(InitDlg)/sizeof(InitDlg[0])),0,0,EditShareDialogProc,0);
        if(DlgCode==11)
        {
          wchar_t CommentNew[MAX_COMMENT];
          MultiByteToWideChar(CP_OEMCP,0,DialogItems[4].Data,-1,CommentNew,sizeof(CommentNew)/sizeof(CommentNew[0]));
          info->shi502_remark=(LPTSTR)CommentNew;
          if(DialogItems[6].Selected)
            info->shi502_max_uses=0xffffffff;
          else
            info->shi502_max_uses=FSF.atoi(DialogItems[8].Data);
          if(NetShareSetInfo(panel->computer_ptr,GetWideNameFromUserData(PInfo.PanelItems[PInfo.CurrentItem].UserData),502,(LPBYTE)info,NULL)==NERR_Success)
            res=true;
        }
        NetApiBufferFree(info);
      }
    }
  }
  return res;
}

long WINAPI NewShareDialogProc(HANDLE hDlg,int Msg,int Param1,long Param2)
{
  switch(Msg)
  {
    case DN_INITDIALOG:
      Info.SendDlgMessage(hDlg,DM_SETTEXTLENGTH,2,MAX_PATH-1);
      break;
  }
  return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}

bool AddShare(UserManager *panel)
{
  bool res=false;
  //Show dialog
  /*
    0000000000111111111122222222223333333333444444444455555555556666666666777777
    0123456789012345678901234567890123456789012345678901234567890123456789012345
  00                                                                            00
  01   ���������������������������� New Share �����������������������������ͻ   01
  02   � Share name                                                         �   02
  03   � TEMP                                                              |�   03
  04   ��������������������������������������������������������������������Ķ   04
  05   �                 [ OK ]                  [ Cancel ]                 �   05
  06   ��������������������������������������������������������������������ͼ   06
  07                                                                            07
    0000000000111111111122222222223333333333444444444455555555556666666666777777
    0123456789012345678901234567890123456789012345678901234567890123456789012345
  */
  static char *NewShareHistoryName="UserManager\\NewShare";
  static struct InitDialogItem InitDlg[]={
  /* 0*/  {DI_DOUBLEBOX,3,1,72,6,0,0,0,0,(char *)mShareNewShare},
  /* 1*/  {DI_TEXT,5,2,0,0,0,0,0,0,(char *)mShareShareName},
  /* 2*/  {DI_EDIT,5,3,70,0,1,(DWORD)NewShareHistoryName,DIF_HISTORY,0,""},
  /* 3*/  {DI_TEXT,5,4,0,0,0,0,DIF_BOXCOLOR|DIF_SEPARATOR,0,""},
  /* 4*/  {DI_BUTTON,0,5,0,0,0,0,DIF_CENTERGROUP,1,(char *)mButtonOk},
  /* 5*/  {DI_BUTTON,0,5,0,0,0,0,DIF_CENTERGROUP,0,(char *)mButtonCancel},
  };
  struct FarDialogItem DialogItems[sizeof(InitDlg)/sizeof(InitDlg[0])];
  InitDialogItems(InitDlg,DialogItems,sizeof(InitDlg)/sizeof(InitDlg[0]));
  strcpy(DialogItems[2].Data,FSF.PointToName(panel->hostfile_oem));
  int DlgCode=Info.DialogEx(Info.ModuleNumber,-1,-1,76,8,"NewShare",DialogItems,(sizeof(InitDlg)/sizeof(InitDlg[0])),0,0,NewShareDialogProc,0);
  if(DlgCode==4)
  {
    wchar_t Share[MAX_PATH];
    MultiByteToWideChar(CP_OEMCP,0,DialogItems[2].Data,-1,Share,sizeof(Share)/sizeof(Share[0]));
    if(panel->level==levelPrinterShared)
    {
      HANDLE printer; PRINTER_DEFAULTSW defaults; PRINTER_INFO_2W *data=NULL;
      memset(&defaults,0,sizeof(defaults));
      defaults.DesiredAccess=PRINTER_ALL_ACCESS;
      if(OpenPrinterW(panel->hostfile,&printer,&defaults))
      {
        DWORD Needed;
        if(!GetPrinterW(printer,2,NULL,0,&Needed))
        {
          if(GetLastError()==ERROR_INSUFFICIENT_BUFFER)
          {
            data=(PRINTER_INFO_2W *)malloc(Needed);
            if(data)
            {
              if(GetPrinterW(printer,2,(PBYTE)data,Needed,&Needed))
              {
                data->Attributes|=PRINTER_ATTRIBUTE_SHARED;
                data->pShareName=Share;
                if(SetPrinterW(printer,2,(PBYTE)data,0)) res=true;
              }
              free(data); data=NULL;
            }
          }
        }
        ClosePrinter(printer);
      }
    }
    else
    {
      SHARE_INFO_2 info;
      info.shi2_netname=(LPTSTR)Share;
      info.shi2_type=STYPE_DISKTREE;
      info.shi2_remark=NULL;
      info.shi2_permissions=0;
      info.shi2_max_uses=0xFFFFFFFF;
      info.shi2_current_uses=0;
      info.shi2_path=(panel->computer_ptr)?((LPTSTR)panel->domain):((LPTSTR)panel->hostfile);
      info.shi2_passwd=NULL;
      if(NetShareAdd(panel->computer_ptr,2,(LPBYTE)&info,NULL)==NERR_Success)
        res=true;
    }
  }
  return res;
}