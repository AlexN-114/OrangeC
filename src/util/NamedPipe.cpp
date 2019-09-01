
/* Software License Agreement
 *
 *     Copyright(C) 1994-2019 David Lindauer, (LADSoft)
 *
 *     This file is part of the Orange C Compiler package.
 *
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     The Orange C Compiler package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 *
 */

#include "Utils.h"
#include <windows.h>
#include <stdlib.h>
#ifndef HAVE_UNISTD_H
#include "io.h"
#include "fcntl.h"
#endif

bool Utils::NamedPipe(int fds[2], const std::string& pipeName)
{
    char pipe[MAX_PATH];
    sprintf(pipe, "\\\\.\\pipe\\%s", pipeName.c_str(), std::string::npos);
#ifndef HAVE_UNISTD_H
    HANDLE handle;
    handle = CreateFile(pipe, GENERIC_READ, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE)
    {
        HANDLE handle1 = CreateFile(pipe, GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle1 != INVALID_HANDLE_VALUE)
        {
            fds[0] = _open_osfhandle((long)handle, O_RDONLY);
            fds[1] = _open_osfhandle((long)handle1, O_WRONLY);
            return true;
        }
        CloseHandle(handle);
    }
#endif
    return false;

}
bool Utils::PipeWrite(int fileno, const std::string& data)
{
    DWORD n = data.size();
    DWORD read;
#ifndef HAVE_UNISTD_H
    return WriteFile((HANDLE)_get_osfhandle(fileno), &n, sizeof(DWORD), &read, NULL) && WriteFile((HANDLE)_get_osfhandle(fileno), data.c_str(), n, &read, NULL);
#else
    return false;
#endif
}

#ifndef HAVE_UNISTD_H
static void WaitForPipeData(HANDLE hPipe, int size)
{
    int xx = GetTickCount();
    while (xx + 10000 > GetTickCount())
    {
        DWORD avail;
        if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &avail, NULL))
        {
            break;
        }
        if (avail >= size)
        {
            return;
        }
        Sleep(0);
    }
}
#endif

std::string Utils::PipeRead(int fileno)
{
#ifndef HAVE_UNISTD_H
    HANDLE hPipe = (HANDLE)_get_osfhandle(fileno);
    WaitForPipeData(hPipe, sizeof(DWORD));
    int n = sizeof(DWORD);
    DWORD read = 0;
    WaitForPipeData(hPipe, n);
    char *buffer = (char *)calloc(1, n + 1);
    if (ReadFile(hPipe, buffer, n, &read, nullptr) && read == n);
    {
        std::string rv = buffer;
        free(buffer);
        return rv;
    }
    free(buffer);
#endif;
    return "";
}

