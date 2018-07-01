/* Software License Agreement
 *
 *     Copyright(C) 1994-2018 David Lindauer, (LADSoft)
 *
 *     This file is part of the Orange C Compiler package.
 *
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version, with the addition of the
 *     Orange C "Target Code" exception.
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

#include "Bitmap.h"
#include "RCFile.h"
#include "ResourceData.h"
#include "ResFile.h"
#include <windows.h>

#include <stdexcept>

#define BITMAP_SIG 0x4d42  // BM
static const int SKIP = 14;

Bitmap::~Bitmap() { delete data; }

void Bitmap::WriteRes(ResFile& resFile)
{
    Resource::WriteRes(resFile);
    if (data)
        data->WriteRes(resFile);
    resFile.Release();
}
void Bitmap::SetData(ResourceData* rdata)
{

    delete data;
    data = rdata;
}
void Bitmap::ReadRC(RCFile& rcFile)
{
    resInfo.SetFlags((resInfo.GetFlags() & ~ResourceInfo::Discardable) | ResourceInfo::Pure);
    resInfo.ReadRC(rcFile, false);
    ResourceData* rd = new ResourceData;
    rd->ReadRC(rcFile);
    if (rd->GetLen() <= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
    {
        delete rd;
        throw new std::runtime_error("invalid bitmap file");
    }
    else
    {
        LPBITMAPFILEHEADER f = (LPBITMAPFILEHEADER)(rd->GetData());
        LPBITMAPINFOHEADER p = (LPBITMAPINFOHEADER)(rd->GetData() + sizeof(BITMAPFILEHEADER));
        if (f->bfType != BITMAP_SIG || f->bfSize != rd->GetLen() ||
            f->bfOffBits < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) || f->bfOffBits >= rd->GetLen() ||
            p->biSize != sizeof(BITMAPINFOHEADER))
        {
            delete rd;
            throw new std::runtime_error("invalid bitmap file");
        }
        // borland sets the size explicitly for RGB images that don't have a size
        // even though windows docs explicitly say it is not required.
        // and I figure there must be a good reason so I'm doing it too...
        if (p->biSizeImage == 0 && p->biCompression == BI_RGB)
        {
            int n = ((((p->biWidth * p->biBitCount) + 31) & ~31) >> 3) * p->biHeight;
            p->biSizeImage = n;
        }
        data = new ResourceData((unsigned char*)p, rd->GetLen() - sizeof(BITMAPFILEHEADER));
        delete rd;
    }
    rcFile.NeedEol();
}
