// WMFPreviewDoc.cpp : Implementation of the WMFPreviewDoc class

#include "stdafx.h"
#include <propkey.h>
#include "WMFPreviewDoc.h"

HRESULT WMFPreviewDoc::LoadFromStream(IStream* pStream, DWORD grfMode)
{
	BOOL bShdr = FALSE;

	ULONG len = 0;
	STATSTG stat;

	if (pStream->Stat(&stat, STATFLAG_DEFAULT) != S_OK)
	{
		return S_FALSE;
	}

	if (stat.cbSize.QuadPart < WMFSPECIALHEADERSIZE) return S_FALSE;

	// First check if there's a special WMF header
	if (pStream->Read(&m_SpecialWMFHeader, WMFSPECIALHEADERSIZE, &len) == S_OK)
	{
		if (len == WMFSPECIALHEADERSIZE)
		{
			if (m_SpecialWMFHeader.Key == WMFSPECIALHEADERKEY)
			{
				// Has special WMF header
				bShdr = TRUE;
			}

			LARGE_INTEGER pos;
			pos.QuadPart = 0;

			if (bShdr) pos.QuadPart = WMFSPECIALHEADERSIZE;

			if (pStream->Seek(pos, STREAM_SEEK_SET, NULL) == S_OK)
			{
				METAHEADER hdr;

				// Read WMF header
				if (pStream->Read(&hdr, sizeof(hdr), &len) == S_OK)
				{
					if (len == sizeof(hdr))
					{
						// Type should be 1 or 2 if valid WMF file
						if ((hdr.mtType != 1) && (hdr.mtType != 2))
						{
							// Probably not a WMF file
							return S_FALSE;
						}
						else
						{
							LARGE_INTEGER pos;
							pos.QuadPart = 0;

							// Go back to beginning and read entire file
							if (pStream->Seek(pos, STREAM_SEEK_SET, NULL) == S_OK)
							{
								BYTE * data = new BYTE[(ULONG) stat.cbSize.QuadPart];

								if (pStream->Read(data, (ULONG) stat.cbSize.QuadPart, &len) == S_OK)
								{
									if (len == stat.cbSize.QuadPart)
									{
										HENHMETAFILE mfile = NULL;

										if (bShdr)
										{
											// Old style windows meta file
											mfile = SetWinMetaFileBits((ULONG) stat.cbSize.QuadPart - WMFSPECIALHEADERSIZE, data + WMFSPECIALHEADERSIZE, NULL, NULL);
										}
										else
										{
											// Enhanced windows meta file
											mfile = SetEnhMetaFileBits((ULONG) stat.cbSize.QuadPart, data);
										}

										if (mfile != NULL)
										{
											m_hMetaFile = mfile;
											m_HasSpecialWMFHeader = bShdr;

											delete data;

											return S_OK;
										}
									}
								}

								delete data;
							}
						}
					}
				}
			}
		}
	}

	return S_FALSE;
}

void WMFPreviewDoc::InitializeSearchContent()
{
}

void WMFPreviewDoc::SetSearchContent(CString& value)
{
}

BOOL WMFPreviewDoc::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
	return CAtlDocumentImpl::GetThumbnail(cx, phbmp, pdwAlpha);

/*	Instead of the above line, replacement function below; CreateCompatibleBitmap() uses the original DC, not the compatible dc

	// In case no valid WMF was loaded return
	if (m_hMetaFile == NULL) return FALSE;

	HDC hdc = ::GetDC(NULL);
	HDC memDC = CreateCompatibleDC(hdc);

	if (memDC != NULL)
	{
		HBITMAP memBM = CreateCompatibleBitmap(hdc, cx, cx);

		if (memBM != NULL)
		{
			HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

			HBRUSH hDrawBrush = CreateSolidBrush(RGB(255, 255, 255));

			RECT rcBounds;
			SetRect(&rcBounds, 0, 0, cx, cx);

			// Fill background with white
			FillRect(memDC, &rcBounds, hDrawBrush);

			int margin = 4;

			rcBounds.left += margin;
			rcBounds.top += margin;
			rcBounds.right -= margin;
			rcBounds.bottom -= margin;

			// Draw WMF file
			OnDrawThumbnail(memDC, &rcBounds);

			SelectObject(memDC, oldBM);

			DeleteObject(hDrawBrush);
			DeleteDC(memDC);
			ReleaseDC(NULL, hdc);

			*phbmp = memBM;

			return TRUE;
		}

		DeleteDC(memDC);
	}

	ReleaseDC(NULL, hdc);

	return FALSE;*/
}

BOOL WMFPreviewDoc::DrawWMF(HDC hdc, LPRECT lprcBounds, COLORREF clrBack)
{
	HDC memDC = CreateCompatibleDC(hdc);

	if (memDC != NULL)
	{
		HBITMAP memBM = CreateCompatibleBitmap(hdc, lprcBounds->right - lprcBounds->left, lprcBounds->bottom - lprcBounds->top);

		if (memBM != NULL)
		{
			HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

			HBRUSH hDrawBrush = CreateSolidBrush(clrBack);

			// Fill background with white
			FillRect(memDC, lprcBounds, hDrawBrush);

			RECT crect;
			int margin = 10;

			crect.left = lprcBounds->left + margin;
			crect.top = lprcBounds->top + margin;
			crect.right = lprcBounds->right - margin;
			crect.bottom = lprcBounds->bottom - margin;

			// Draw WMF file
			OnDrawThumbnail(memDC, &crect);

			// Copy to main paint DC
			if (BitBlt(hdc, lprcBounds->left, lprcBounds->top, lprcBounds->right - lprcBounds->left, lprcBounds->bottom - lprcBounds->top, memDC, 0, 0, SRCCOPY))
			{
				SelectObject(memDC, oldBM);

				DeleteObject(memBM);
				DeleteObject(hDrawBrush);
				DeleteDC(memDC);

				return TRUE;
			}

			SelectObject(memDC, oldBM);

			DeleteObject(memBM);
			DeleteObject(hDrawBrush);
		}

		DeleteDC(memDC);
	}

	return FALSE;
}

void WMFPreviewDoc::OnDrawThumbnail(HDC hDrawDC, LPRECT lprcBounds)
{
#ifdef _DEBUG

	CString strDebug = _T("Debug Info:");

#endif

	if (m_hMetaFile != NULL)
	{
		SIZE msize;

		if (m_HasSpecialWMFHeader)
		{
			msize.cx = m_SpecialWMFHeader.Right - m_SpecialWMFHeader.Left;
			msize.cy = m_SpecialWMFHeader.Bottom - m_SpecialWMFHeader.Top;
		}
		else
		{
			UINT hdrlen = GetEnhMetaFileHeader(m_hMetaFile, 0, NULL);

			BYTE * buf = new BYTE[hdrlen];

			if (GetEnhMetaFileHeader(m_hMetaFile, hdrlen, (ENHMETAHEADER*)buf) != 0)
			{
				ENHMETAHEADER* mfhdr = (ENHMETAHEADER*)buf;

				msize.cx = mfhdr->rclFrame.right - mfhdr->rclFrame.left;
				msize.cy = mfhdr->rclFrame.bottom - mfhdr->rclFrame.top;
			}

			delete buf;
		}

		// Center WMF in output rectangle
		int dw = lprcBounds->right - lprcBounds->left;
		int dh = lprcBounds->bottom - lprcBounds->top;

		int w2 = dw;
		int h2 = dh;

		if (msize.cx > msize.cy)
		{
			h2 = (int) (w2 * (float) ((float) msize.cy / (float) msize.cx));

			if (h2 > dh)
			{
				w2 = (int) (w2 * (float) ((float) dh / (float) h2));
				h2 = dh;
			}
		}
		else
		{
			w2 = (int) (h2 * (float) ((float) msize.cx / (float) msize.cy));

			if (w2 > dw)
			{
				h2 = (int) (h2 * (float) ((float) dw / (float) w2));
				w2 = dw;
			}
		}

#ifdef _DEBUG

		char buffer[65];
		_itoa_s(msize.cx, buffer, 65, 10);
		strDebug += _T(" WMFRECT: ");
		strDebug += buffer;
		_itoa_s(msize.cy, buffer, 65, 10);
		strDebug += _T(" / ");
		strDebug += buffer;

		strDebug += _T(" WNDRECT: ");
		_itoa_s(dw, buffer, 65, 10);
		strDebug += _T(" / ");
		strDebug += buffer;
		_itoa_s(dh, buffer, 65, 10);
		strDebug += _T(" / ");
		strDebug += buffer;

		strDebug += _T(" DRWRECT: ");
		_itoa_s(w2, buffer, 65, 10);
		strDebug += _T(" / ");
		strDebug += buffer;
		_itoa_s(h2, buffer, 65, 10);
		strDebug += _T(" / ");
		strDebug += buffer;

#endif

		RECT prect;
		prect.left = ((dw - w2) / 2) + lprcBounds->left;
		prect.top = (dh - h2) / 2 + lprcBounds->top;
		prect.right = prect.left + w2;
		prect.bottom = prect.top + h2;
			
		PlayEnhMetaFile(hDrawDC, m_hMetaFile, &prect);
	}
#ifdef _DEBUG
	else strDebug += _T(" NO_WMF_FILE");

	HFONT hStockFont = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
	LOGFONT lf;

	GetObject(hStockFont, sizeof(LOGFONT), &lf);
	lf.lfHeight = 34;

	HFONT hDrawFont = CreateFontIndirect(&lf);
	HFONT hOldFont = (HFONT) SelectObject(hDrawDC, hDrawFont);

	int bdept = GetDeviceCaps(hDrawDC, BITSPIXEL);

	SetTextColor(hDrawDC, RGB(255, 0, 0));

	char buffer[65];
	_itoa_s(bdept, buffer, 65, 10);
	strDebug += _T(" ");
	strDebug += buffer;
	strDebug += _T("BIT");

	DrawText(hDrawDC, strDebug, strDebug.GetLength(), lprcBounds, DT_CENTER | DT_WORDBREAK);

	SelectObject(hDrawDC, hDrawFont);
	SelectObject(hDrawDC, hOldFont);

	DeleteObject(hDrawFont);
#endif
}
