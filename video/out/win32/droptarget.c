/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <ole2.h>
#include <shobjidl.h>

#include "common/msg.h"
#include "common/common.h"
#include "input/input.h"
#include "input/event.h"
#include "osdep/atomic.h"
#include "osdep/io.h"
#include "osdep/windows_utils.h"
#include "mpv_talloc.h"

#include "droptarget.h"

struct droptarget {
    IDropTarget iface;
    atomic_int ref_cnt;
    struct mp_log *log;
    struct input_ctx *input_ctx;
    DWORD last_effect;
    IDataObject *data_obj;
};

static FORMATETC fmtetc_file = {
    .cfFormat = CF_HDROP,
    .dwAspect = DVASPECT_CONTENT,
    .lindex = -1,
    .tymed = TYMED_HGLOBAL,
};

static FORMATETC fmtetc_url = {
    .dwAspect = DVASPECT_CONTENT,
    .lindex = -1,
    .tymed = TYMED_HGLOBAL,
};

static void DropTarget_Destroy(struct droptarget *t)
{
    SAFE_RELEASE(t->data_obj);
    talloc_free(t);
}

static STDMETHODIMP DropTarget_QueryInterface(IDropTarget *self, REFIID riid,
                                              void **ppvObject)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) {
        *ppvObject = self;
        IDropTarget_AddRef(self);
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static STDMETHODIMP_(ULONG) DropTarget_AddRef(IDropTarget *self)
{
    struct droptarget *t = (struct droptarget *)self;
    return atomic_fetch_add(&t->ref_cnt, 1) + 1;
}

static STDMETHODIMP_(ULONG) DropTarget_Release(IDropTarget *self)
{
    struct droptarget *t = (struct droptarget *)self;

    ULONG ref_cnt = atomic_fetch_add(&t->ref_cnt, -1) - 1;
    if (ref_cnt == 0)
        DropTarget_Destroy(t);
    return ref_cnt;
}

static STDMETHODIMP DropTarget_DragEnter(IDropTarget *self,
                                         IDataObject *pDataObj,
                                         DWORD grfKeyState, POINTL pt,
                                         DWORD *pdwEffect)
{
    struct droptarget *t = (struct droptarget *)self;

    IDataObject_AddRef(pDataObj);
    if (FAILED(IDataObject_QueryGetData(pDataObj, &fmtetc_file)) &&
        FAILED(IDataObject_QueryGetData(pDataObj, &fmtetc_url)))
    {
        *pdwEffect = DROPEFFECT_NONE;
    }

    SAFE_RELEASE(t->data_obj);
    t->data_obj = pDataObj;
    t->last_effect = *pdwEffect;
    return S_OK;
}

static STDMETHODIMP DropTarget_DragOver(IDropTarget *self, DWORD grfKeyState,
                                        POINTL pt, DWORD *pdwEffect)
{
    struct droptarget *t = (struct droptarget *)self;

    *pdwEffect = t->last_effect;
    return S_OK;
}

static STDMETHODIMP DropTarget_DragLeave(IDropTarget *self)
{
    struct droptarget *t = (struct droptarget *)self;

    SAFE_RELEASE(t->data_obj);
    return S_OK;
}

static STDMETHODIMP DropTarget_Drop(IDropTarget *self, IDataObject *pDataObj,
                                    DWORD grfKeyState, POINTL pt,
                                    DWORD *pdwEffect)
{
    struct droptarget *t = (struct droptarget *)self;
    enum mp_dnd_action action = (grfKeyState & MK_SHIFT) ? DND_APPEND : DND_REPLACE;

    SAFE_RELEASE(t->data_obj);

    STGMEDIUM medium;
    if (SUCCEEDED(IDataObject_GetData(pDataObj, &fmtetc_file, &medium))) {
        if (GlobalLock(medium.hGlobal)) {
            HDROP drop = medium.hGlobal;

            UINT files_num = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
            char **files = talloc_zero_array(NULL, char*, files_num);

            UINT recvd_files = 0;
            for (UINT i = 0; i < files_num; i++) {
                UINT len = DragQueryFileW(drop, i, NULL, 0);
                wchar_t *buf = talloc_array(NULL, wchar_t, len + 1);

                if (DragQueryFileW(drop, i, buf, len + 1) == len) {
                    char *fname = mp_to_utf8(files, buf);
                    files[recvd_files++] = fname;

                    MP_VERBOSE(t, "received dropped file: %s\n", fname);
                } else {
                    MP_ERR(t, "error getting dropped file name\n");
                }

                talloc_free(buf);
            }

            GlobalUnlock(medium.hGlobal);
            mp_event_drop_files(t->input_ctx, recvd_files, files, action);
            talloc_free(files);
        }

        ReleaseStgMedium(&medium);
    } else if (SUCCEEDED(IDataObject_GetData(pDataObj, &fmtetc_url, &medium))) {
        wchar_t *wurl = GlobalLock(medium.hGlobal);
        if (wurl) {
            char *url = mp_to_utf8(NULL, wurl);
            if (mp_event_drop_mime_data(t->input_ctx, "text/uri-list",
                                        bstr0(url), action) > 0)
            {
                MP_VERBOSE(t, "received dropped URL: %s\n", url);
            } else {
                MP_ERR(t, "error getting dropped URL\n");
            }

            talloc_free(url);
            GlobalUnlock(medium.hGlobal);
        }

        ReleaseStgMedium(&medium);
    } else {
        t->last_effect = DROPEFFECT_NONE;
    }

    *pdwEffect = t->last_effect;
    return S_OK;
}

static IDropTargetVtbl idroptarget_vtbl = {
    .QueryInterface = DropTarget_QueryInterface,
    .AddRef = DropTarget_AddRef,
    .Release = DropTarget_Release,
    .DragEnter = DropTarget_DragEnter,
    .DragOver = DropTarget_DragOver,
    .DragLeave = DropTarget_DragLeave,
    .Drop = DropTarget_Drop,
};

IDropTarget *mp_w32_droptarget_create(struct mp_log *log,
                                      struct input_ctx *input_ctx)
{
    fmtetc_url.cfFormat = RegisterClipboardFormatW(L"UniformResourceLocatorW");

    struct droptarget *dt = talloc(NULL, struct droptarget);
    dt->iface.lpVtbl = &idroptarget_vtbl;
    atomic_store(&dt->ref_cnt, 0);
    dt->last_effect = 0;
    dt->data_obj = NULL;
    dt->log = mp_log_new(dt, log, "droptarget");
    dt->input_ctx = input_ctx;

    return &dt->iface;
}
