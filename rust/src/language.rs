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

use std::collections::HashSet;
use std::ffi::{c_char,c_int,c_uchar,CStr};

use isolang::Language;
use language_tags::LanguageTag;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bstr {
    ptr: *mut c_uchar,
    len: usize,
}

pub mod track_flags {
    use std::ffi::c_int;

    pub const NONE: c_int = 0;
    pub const HEARING_IMPAIRED: c_int = 1 << 2;
    pub const VISUAL_IMPAIRED: c_int = 1 << 3;
    pub const ATTACHED_PICTURE: c_int = 1 << 4;
    pub const FORCED: c_int = 1 << 5;
}

impl Bstr {
    fn as_str(&self) -> &str {
        unsafe {
            std::str::from_utf8(std::slice::from_raw_parts(self.ptr, self.len))
                .expect("Invalid UTF-8 found in Bstr")
        }
    }

    const fn empty() -> Self {
        Self { ptr: std::ptr::null_mut(), len: 0 }
    }

    fn splice(&self, start: c_int, end: c_int) -> Self {
        extern "C" { fn bstr_splice(str: Bstr, start: c_int, end: c_int) -> Bstr; }
        unsafe { bstr_splice(*self, start, end) }
    }
}

fn calculate_score(lang: &LanguageTag, target: &LanguageTag) -> c_int
{
    let lang = lang.canonicalize().unwrap_or_else(|_| lang.clone());
    let target = target.canonicalize().unwrap_or_else(|_| target.clone());

    let mut score = c_int::MAX;
    if lang == target {
        return score;
    }

    let primary_lang = lang.primary_language();
    let primary_target = target.primary_language();
    let parse = |s: &str| s.parse::<Language>().ok();
    if primary_lang != primary_target && parse(primary_lang) != parse(primary_target) {
        return 0;
    }

    const PENALTY: c_int = 1000;

    type LanguageCheck = fn(&LanguageTag) -> Option<&str>;
    static CHECKS: &[(LanguageCheck, i32)] = &[
        (LanguageTag::region, PENALTY),
        (LanguageTag::script, PENALTY),
        (LanguageTag::extended_language, PENALTY),
        (LanguageTag::private_use, PENALTY),
    ];

    for &(getter, penalty) in CHECKS {
        if let Some(val) = getter(&target) {
            if getter(&lang) != Some(val) {
                score -= penalty;
            }
        }
    }

    let target_vs: HashSet<_> = target.variant_subtags().collect();
    let lang_vs: HashSet<_> = lang.variant_subtags().collect();
    score -= target_vs.difference(&lang_vs).count() as c_int * PENALTY;

    let target_es: HashSet<_> = target.extension_subtags().collect();
    let lang_es: HashSet<_> = lang.extension_subtags().collect();
    score -= target_es.difference(&lang_es).count() as c_int * PENALTY;

    score
}

/// Calculates a score indicating how well a target language tag matches a list of language tags.
///
/// # Parameters
/// * `langs` - A null-terminated array of C strings, each representing a language tag.
/// * `target` - A C string representing the target language tag to match against.
///
/// # Returns
/// An integer score indicating the match quality:
/// * `c_int::MAX` - Perfect match
/// * `0` - No match
/// * Other positive values - Partial match, with higher values indicating better matches
///
/// # Safety
/// This function is unsafe because it dereferences raw pointers.
/// The `langs` parameter must be a null-terminated array of valid C strings.
/// The `target` parameter must be a valid C string.
///
/// # Examples
/// ```c
/// const char *langs[] = {"en-US", "fr-FR", "de-DE", NULL};
/// int score = mp_match_lang(langs, "en-GB");
/// ```
#[no_mangle]
pub unsafe extern "C" fn mp_match_lang(langs: *const *const c_char, target: *const c_char) -> c_int
{
    if langs.is_null() || target.is_null() {
        return 0;
    }

    let target_str = match CStr::from_ptr(target).to_str() {
        Ok(s) if !s.is_empty() => s,
        _ => return 0,
    };
    let target_tag = LanguageTag::parse(target_str).ok();

    std::iter::successors(Some((langs, 0)), |&(lang, i)| Some((lang.add(1), i + 1)))
        .map(|(lang, i)| (*lang, i))
        .take_while(|&(lang, _)| !lang.is_null())
        .filter_map(|(lang, i)| {
            let lang_str = CStr::from_ptr(lang).to_str().ok()?;
            let lang_tag = LanguageTag::parse(lang_str).ok();
            match (lang_tag, target_tag.as_ref()) {
                (Some(lang_tag), Some(target_tag)) => Some(calculate_score(&lang_tag, target_tag) - i),
                // If language tag parsing failed, compare the strings directly
                // Note that in practice it rarely happens, as the language tags
                // are not validated when parsing.
                _ if lang_str == target_str => Some(c_int::MAX),
                _ => None,
            }
        }).max().unwrap_or(0)
}

fn extract_tag<'a, I>(iter: &mut I, end: usize, s: &'a str) -> Option<(usize, &'a str, &'a str)>
where
    I: Iterator<Item = (usize, &'a str)>,
{
    let (pos, delimiter) = iter.next().unwrap_or((0, ""));
    let tag = &s[pos + delimiter.len()..end];
    let stripped = match delimiter {
        "[" => tag.strip_suffix("]"),
        "(" => tag.strip_suffix(")"),
        _ => Some(tag),
    }?;
    Some((pos, stripped, delimiter))
}

fn guess_lang(name: &Bstr) -> (Option<(usize, &str, &str)>, c_int)
{
    let mut lang = None;
    let mut flags = track_flags::NONE;

    if name.len == 0 {
        return (lang, flags);
    }

    let s = name.as_str();
    let mut iter = s.rmatch_indices(&['.', '[', '(']);
    let mut pos = name.len;

    if let Some((cur_pos, _, ".")) = extract_tag(&mut iter, pos, s) {
        pos = cur_pos;
    } else {
        return (lang, flags);
    }

    while let Some((cur_pos, tag, delimiter)) = extract_tag(&mut iter, pos, s) {
        pos = cur_pos;
        match tag {
            "hi" | "sdh" | "cc" => flags |= track_flags::HEARING_IMPAIRED,
            "forced" => flags |= track_flags::FORCED,
            _ => {
                lang = tag.parse::<LanguageTag>()
                    .ok()
                    .and_then(|t| t.canonicalize().unwrap_or(t).primary_language().parse::<Language>().ok())
                    .is_some()
                    .then_some((pos, tag, delimiter));
                break;
            }
        }
    }

    (lang, flags)
}

unsafe fn assign_ptr<T>(ptr: *mut T, value: T) {
    if !ptr.is_null() {
        *ptr = value;
    }
}

/// Attempts to extract language information from a filename.
///
/// This function parses a filename to identify language tags and hearing-impaired indicators.
/// It looks for patterns like "filename.en.mp4" or "filename.en.hi.mp4" where "en" is the
/// language code and "hi" indicates hearing-impaired content.
///
/// # Parameters
/// * `name` - A `Bstr`` containing the filename to analyze.
/// * `lang_start` - An optional pointer to store the starting position of the
///   language tag in the filename. Including the delimiter.
///   Set to -1 if no language tag is found.
/// * `flags` - An optional pointer to store inferred track flags.
///
/// # Returns
/// A `Bstr`` containing the extracted language tag, or an empty `Bstr` if no
/// language tag is found.
///
/// # Safety
/// This function is unsafe because it dereferences raw pointers.
/// The `lang_start` and `hearing_impaired` pointers must be either null or valid pointers.
///
/// # Examples
/// For a filename like "movie.en.mp4", the function would:
/// * Return a `Bstr` containing "en"
/// * Set `lang_start` to the position of ".en" in the string
/// * Set `flags` to `track_flags::NONE`
///
/// For a filename like "movie.en.hi.mp4", the function would:
/// * Return a `Bstr` containing "en"
/// * Set `lang_start` to the position of ".en" in the string
/// * Set `flags` to `track_flags::HEARING_IMPAIRED`
#[no_mangle]
pub unsafe extern "C" fn mp_guess_lang_from_filename(name: Bstr, lang_start: *mut c_int,
                                                     flags: *mut c_int) -> Bstr
{
    let (lang, f) = guess_lang(&name);
    assign_ptr(flags, f);
    if let Some((pos, tag, delimiter)) = lang {
        assign_ptr(lang_start, pos as c_int);
        let start = pos + delimiter.len();
        let end = start + tag.len();
        debug_assert!(start <= name.len && end <= name.len);
        name.splice(start as c_int, end as c_int)
    } else {
        assign_ptr(lang_start, -1);
        Bstr::empty()
    }
}
