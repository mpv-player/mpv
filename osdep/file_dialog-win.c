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

#include "file_dialog.h"

#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>

#include <mpv_talloc.h>
#include <osdep/io.h>
#include <player/core.h>

#include "windows_utils.h"

static int append_wchar(void *talloc_ctx, wchar_t **pattern, size_t end, const char *str)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (count <= 0)
        return 0;

    MP_TARRAY_GROW(talloc_ctx, *pattern, end + count);
    return MultiByteToWideChar(CP_UTF8, 0, str, -1, *pattern + end, count);
}

static wchar_t *create_extensions_pattern(void *talloc_ctx, char **extension)
{
    if (!extension)
        return NULL;

    wchar_t *pattern = NULL;
    size_t end = 0;
    int len;

    while (*extension) {
        len = append_wchar(talloc_ctx, &pattern, end, "*.");
        if (!len)
            return NULL;
        end += len - 1;  // remove the null terminator

        len = append_wchar(talloc_ctx, &pattern, end, *extension);
        if (!len)
            return NULL;
        end += len;
        extension++;
        pattern[end - 1] = *extension ? ';' : 0;
    }

    return pattern;
}

char **mp_file_dialog_get_files(void *talloc_ctx, const mp_file_dialog_params *params)
{
    if (!str_in_list(bstr0("native"), params->providers))
        return NULL;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return NULL;

    void *tmp = talloc_new(NULL);
    char **ret = NULL;

    IFileDialog *file_dialog = NULL;
    IFileOpenDialog *file_open_dialog = NULL;
    IShellItemArray *item_array = NULL;
    bool save = params->flags & MP_FILE_DIALOG_SAVE;
    hr = CoCreateInstance(save ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog,
                          NULL, CLSCTX_ALL, &IID_IFileDialog, (void **)&file_dialog);
    if (FAILED(hr))
        goto done;

    const mp_file_dialog_filters *filter = params->filters;
    if (filter && filter->name) {
        size_t count = 0;
        COMDLG_FILTERSPEC *file_types = NULL;
        // Always add "All Files" filter
        MP_TARRAY_APPEND(tmp, file_types, count, (COMDLG_FILTERSPEC) {
            .pszName = L"All Files",
            .pszSpec = L"*"
        });
        while (filter->name) {
            wchar_t *extensions = create_extensions_pattern(tmp, filter->extensions);
            if (!extensions || !extensions[0])
                continue;
            MP_TARRAY_APPEND(tmp, file_types, count, (COMDLG_FILTERSPEC) {
                .pszName = mp_from_utf8(tmp, filter->name),
                .pszSpec = extensions,
            });
            filter++;
        }
        if (count)
            IFileDialog_SetFileTypes(file_dialog, count, file_types);
    }

    if (params->initial_dir) {
        IShellItem *psi = NULL;
        hr = SHCreateItemFromParsingName(mp_from_utf8(tmp, params->initial_dir),
                                         NULL, &IID_IShellItem, (void **)&psi);
        if (SUCCEEDED(hr)) {
            IFileDialog_SetFolder(file_dialog, psi);
            IShellItem_Release(psi);
        }
    }

    if (params->title)
        IFileDialog_SetTitle(file_dialog, mp_from_utf8(tmp, params->title));

    if (params->initial_selection)
        IFileDialog_SetFileName(file_dialog, mp_from_utf8(tmp, params->initial_selection));

    DWORD options = 0;
    IFileDialog_GetOptions(file_dialog, &options);
    if (params->flags & MP_FILE_DIALOG_DIRECTORY)
        options |= FOS_PICKFOLDERS;
    if (params->flags & MP_FILE_DIALOG_MULTIPLE)
        options |= FOS_ALLOWMULTISELECT;
    IFileDialog_SetOptions(file_dialog, options | FOS_FORCEFILESYSTEM);

    HWND parent = params->parent ? (HWND)(intptr_t)(*(int64_t*)params->parent) : NULL;
    hr = IFileDialog_Show(file_dialog, parent);
    if (FAILED(hr))
        goto done;

    DWORD itemCount = 0;

    if (params->flags & MP_FILE_DIALOG_MULTIPLE) {
        hr = IFileDialog_QueryInterface(file_dialog, &IID_IFileOpenDialog,
                                        (void **)&file_open_dialog);
        if (FAILED(hr))
            goto done;

        hr = IFileOpenDialog_GetResults(file_open_dialog, &item_array);
        if (FAILED(hr))
            goto done;

        hr = IShellItemArray_GetCount(item_array, &itemCount);
        if (FAILED(hr))
            goto done;
    } else {
        itemCount = 1;
    }

    size_t ret_count = 0;
    for (DWORD i = 0; i < itemCount; i++) {
        IShellItem *pItem = NULL;

        if (params->flags & MP_FILE_DIALOG_MULTIPLE) {
            hr = IShellItemArray_GetItemAt(item_array, i, &pItem);
            if (FAILED(hr))
                continue;
        } else {
            hr = IFileDialog_GetResult(file_dialog, &pItem);
            if (FAILED(hr))
                continue;
        }

        wchar_t *path = NULL;
        hr = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &path);
        if (SUCCEEDED(hr)) {
            char *item = mp_to_utf8(NULL, path);
            MP_TARRAY_APPEND(talloc_ctx, ret, ret_count, item);
            talloc_steal(ret, item);
            CoTaskMemFree(path);
        }
        IShellItem_Release(pItem);
    }

    if (ret_count)
        MP_TARRAY_APPEND(talloc_ctx, ret, ret_count, NULL);

    assert(ret_count || !ret);

done:
    SAFE_RELEASE(file_open_dialog);
    SAFE_RELEASE(item_array);
    SAFE_RELEASE(file_dialog);
    CoUninitialize();

    talloc_free(tmp);
    return ret;
}
