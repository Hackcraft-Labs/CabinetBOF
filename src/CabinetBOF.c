#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tlhelp32.h>
#include <fdi.h>
#include "../beacon.h"

DECLSPEC_IMPORT void *__cdecl MSVCRT$malloc(size_t size);
DECLSPEC_IMPORT void __cdecl MSVCRT$free(void *ptr);
DECLSPEC_IMPORT int __cdecl MSVCRT$strncmp(const char *str1, const char *str2, size_t num);
DECLSPEC_IMPORT int __cdecl MSVCRT$sscanf_s(const char *buffer, const char *format, ...);
DECLSPEC_IMPORT void *__cdecl MSVCRT$memcpy(void *dest, const void *src, size_t count);
DECLSPEC_IMPORT int __cdecl MSVCRT$_snprintf(char *buffer, size_t count, const char *format, ...);

DECLSPEC_IMPORT HANDLE WINAPI KERNEL32$CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
DECLSPEC_IMPORT BOOL WINAPI KERNEL32$ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
DECLSPEC_IMPORT BOOL WINAPI KERNEL32$WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
DECLSPEC_IMPORT DWORD WINAPI KERNEL32$SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DECLSPEC_IMPORT BOOL WINAPI KERNEL32$CloseHandle(HANDLE hObject);
DECLSPEC_IMPORT BOOL WINAPI KERNEL32$CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

DECLSPEC_IMPORT HFDI FAR DIAMONDAPI CABINET$FDICreate(PFNALLOC pfnalloc, PFNFREE pfnfree, PFNOPEN pfnopen, PFNREAD pfnread, PFNWRITE pfnwrite, PFNCLOSE pfnclose, PFNSEEK pfnseek, int cpuType, PERF perf);
DECLSPEC_IMPORT BOOL FAR DIAMONDAPI CABINET$FDICopy(HFDI hfdi, char FAR *pszCabinet, char FAR *pszCabPath, int flags, PFNFDINOTIFY pfnfdin, PFNFDIDECRYPT pfnfdid, void FAR *pvUser);
DECLSPEC_IMPORT BOOL FAR DIAMONDAPI CABINET$FDIDestroy(HFDI hfdi);

typedef struct
{
   int is_memory;

   unsigned char *buf;
   size_t size;
   size_t pos;

   HANDLE hFile;
} MY_FD;

void FAR *DIAMONDAPI MemAlloc(ULONG cb)
{
   return MSVCRT$malloc(cb);
}

void FAR DIAMONDAPI MemFree(void FAR *pv)
{
   MSVCRT$free(pv);
}

INT_PTR FAR DIAMONDAPI MemOpen(char FAR *pszFile, int oflag, int pmode)
{
   (void)oflag;
   (void)pmode;

   MY_FD *fd = (MY_FD *)MSVCRT$malloc(sizeof(MY_FD));
   if (!fd)
      return -1;

   if (MSVCRT$strncmp(pszFile, "MEMCAB:", 7) == 0)
   {
      unsigned long long ptr_val, sz_val;
      if (MSVCRT$sscanf_s(pszFile, "MEMCAB:%llu:%llu", &ptr_val, &sz_val) == 2)
      {
         fd->is_memory = 1;
         fd->buf = (unsigned char *)(uintptr_t)ptr_val;
         fd->size = (size_t)sz_val;
         fd->pos = 0;
         fd->hFile = INVALID_HANDLE_VALUE;
         return (INT_PTR)fd;
      }
   }

   fd->is_memory = 0;
   fd->hFile = KERNEL32$CreateFileA(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
   if (fd->hFile == INVALID_HANDLE_VALUE)
   {
      MSVCRT$free(fd);
      return -1;
   }
   return (INT_PTR)fd;
}

UINT FAR DIAMONDAPI MemRead(INT_PTR hf, void FAR *pv, UINT cb)
{
   MY_FD *fd = (MY_FD *)hf;

   if (fd->is_memory)
   {
      size_t left = fd->size - fd->pos;
      size_t to_read = (cb > left) ? left : cb;
      if (to_read > 0)
      {
         MSVCRT$memcpy(pv, fd->buf + fd->pos, to_read);
         fd->pos += to_read;
      }
      return (UINT)to_read;
   }
   else
   {
      DWORD bytesRead = 0;
      KERNEL32$ReadFile(fd->hFile, pv, cb, &bytesRead, NULL);
      return (UINT)bytesRead;
   }
}

UINT FAR DIAMONDAPI MemWrite(INT_PTR hf, void FAR *pv, UINT cb)
{
   MY_FD *fd = (MY_FD *)hf;

   if (fd->is_memory)
   {
      return 0;
   }
   else
   {
      DWORD bytesWritten = 0;
      KERNEL32$WriteFile(fd->hFile, pv, cb, &bytesWritten, NULL);
      return (UINT)bytesWritten;
   }
}

long FAR DIAMONDAPI MemSeek(INT_PTR hf, long dist, int seektype)
{
   MY_FD *fd = (MY_FD *)hf;

   if (fd->is_memory)
   {
      switch (seektype)
      {
      case 0:
         fd->pos = dist;
         break;
      case 1:
         fd->pos += dist;
         break;
      case 2:
         fd->pos = fd->size + dist;
         break;
      }
      return (long)fd->pos;
   }
   else
   {
      DWORD moveMethod;
      switch (seektype)
      {
      case 0:
         moveMethod = FILE_BEGIN;
         break;
      case 1:
         moveMethod = FILE_CURRENT;
         break;
      case 2:
         moveMethod = FILE_END;
         break;
      default:
         return -1;
      }
      return (long)KERNEL32$SetFilePointer(fd->hFile, dist, NULL, moveMethod);
   }
}

int FAR DIAMONDAPI MemClose(INT_PTR hf)
{
   MY_FD *fd = (MY_FD *)hf;

   if (!fd->is_memory && fd->hFile != INVALID_HANDLE_VALUE)
      KERNEL32$CloseHandle(fd->hFile);

   MSVCRT$free(fd);
   return 0;
}

void CreateDirectoriesForPath(char *filePath)
{
   char *p = filePath;

   if (p[0] != '\0' && p[1] == ':')
      p += 2;

   while (*p == '\\' || *p == '/')
      p++;

   while (*p)
   {
      if (*p == '\\' || *p == '/')
      {
         char temp = *p;
         *p = '\0';
         KERNEL32$CreateDirectoryA(filePath, NULL);
         *p = temp;
      }
      p++;
   }
}

INT_PTR FAR DIAMONDAPI NotifyCallback(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin)
{
   switch (fdint)
   {
   case fdintCOPY_FILE:
   {
      char destPath[MAX_PATH];

      MSVCRT$_snprintf(destPath, MAX_PATH, "%s\\%s", (char *)pfdin->pv, pfdin->psz1);

      for (int i = 0; destPath[i] != '\0'; i++)
         if (destPath[i] == '/')
            destPath[i] = '\\';

      CreateDirectoriesForPath(destPath);

      MY_FD *fd = (MY_FD *)MSVCRT$malloc(sizeof(MY_FD));
      if (!fd)
         return -1;

      fd->is_memory = 0;
      fd->hFile = KERNEL32$CreateFileA(destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

      if (fd->hFile == INVALID_HANDLE_VALUE)
      {
         BeaconPrintf(CALLBACK_OUTPUT, "[-] Failed to create output file: %s\n", destPath);
         MSVCRT$free(fd);
         return -1;
      }

      BeaconPrintf(CALLBACK_OUTPUT, "[+] Extracting: %s\n", pfdin->psz1);

      return (INT_PTR)fd;
   }

   case fdintCLOSE_FILE_INFO:
   {
      MY_FD *fd = (MY_FD *)pfdin->hf;

      KERNEL32$CloseHandle(fd->hFile);
      MSVCRT$free(fd);
      return TRUE;
   }

   case fdintCABINET_INFO:
   case fdintENUMERATE:
      return 0;

   case fdintPARTIAL_FILE:
   case fdintNEXT_CABINET:
      BeaconPrintf(CALLBACK_OUTPUT, "[-] Spanning cabinets/partial files are not supported.\n");
      return -1;
   }
   return 0;
}

BOOL ExtractCabFromMemory(unsigned char *cabData, size_t cabSize, const char *outputDir)
{
   ERF erf;
   HFDI hfdi = CABINET$FDICreate(MemAlloc, MemFree, MemOpen, MemRead, MemWrite, MemClose, MemSeek, cpuUNKNOWN, &erf);

   if (hfdi == NULL)
   {
      BeaconPrintf(CALLBACK_OUTPUT, "[-] FDICreate failed. Error code: %d\n", erf.erfOper);
      return FALSE;
   }

   char tempOutDir[MAX_PATH];
   MSVCRT$_snprintf(tempOutDir, MAX_PATH, "%s\\", outputDir);
   CreateDirectoriesForPath(tempOutDir);

   char magicName[256];
   MSVCRT$_snprintf(magicName, sizeof(magicName), "MEMCAB:%llu:%llu",
                    (unsigned long long)(uintptr_t)cabData,
                    (unsigned long long)cabSize);

   BOOL result = CABINET$FDICopy(hfdi, magicName, "", 0, NotifyCallback, NULL, (void *)outputDir);

   if (!result)
   {
      BeaconPrintf(CALLBACK_OUTPUT, "[-] FDICopy failed. Error code: %d\n", erf.erfOper);
   }

   CABINET$FDIDestroy(hfdi);
   return result;
}

void Extract(char *args, int length)
{
   datap parser;
   char *path;
   int cabLen = 0;

   BeaconDataParse(&parser, args, length);
   path = BeaconDataExtract(&parser, NULL);

   char *cabBuffer = BeaconDataExtract(&parser, &cabLen);

   if (cabBuffer == NULL)
   {
      BeaconPrintf(CALLBACK_ERROR, "[!] CAB content buffer missing or null.");
      return;
   }

   BeaconPrintf(CALLBACK_OUTPUT, "Extracting to %s.", path);
   ExtractCabFromMemory(cabBuffer, cabLen, path);
}