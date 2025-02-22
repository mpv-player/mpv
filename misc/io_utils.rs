/*
 * I/O utility functions
 *
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

use std::ffi::c_char;
use std::ffi::CStr;
use std::io::Write;
use std::path::Path;
use tempfile::NamedTempFile;

#[no_mangle]
pub extern "C" fn mp_save_to_file(filepath: *const c_char, data: *const u8, size: usize) -> bool
{
    if filepath.is_null() || data.is_null() || size == 0 {
        return false;
    }

    let c_str = unsafe { CStr::from_ptr(filepath) };
    let target_path = match c_str.to_str() {
        Ok(path) => Path::new(path),
        Err(_) => return false,
    };

    let dir = match target_path.parent() {
        Some(dir) => dir,
        None => return false,
    };

    let filename = match target_path.file_name() {
        Some(name) => name,
        None => return false,
    };

    let mut temp_file = match NamedTempFile::with_prefix_in(filename, dir) {
        Ok(file) => file,
        Err(_) => return false,
    };

    // Write data to temporary file
    let written = temp_file.write(unsafe { std::slice::from_raw_parts(data, size) });
    if written.is_err() {
        return false;
    }

    temp_file.persist(&target_path).is_ok()
}
