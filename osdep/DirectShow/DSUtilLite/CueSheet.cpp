/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "stdafx.h"
#include "CueSheet.h"

#include <algorithm>
#include <sstream>
#include <regex>

using namespace std;

enum class ParserState
{
    GLOBAL,
    FILE,
    TRACK
};

typedef string::value_type char_t;

static char_t up_char(char_t ch)
{
    return use_facet<ctype<char_t>>(locale()).toupper(ch);
}

static string toupper(const string &src)
{
    string result;
    transform(src.begin(), src.end(), back_inserter(result), up_char);
    return result;
}

static void str_replace(string &s, const string &search, const string &replace)
{
    for (string::size_type pos = 0;; pos += replace.length())
    {
        pos = s.find(search, pos);
        if (pos == string::npos)
            break;

        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

static string GetCueParam(string line, bool firstWord = false)
{
    const string delims(" \t\n\r\"'");
    string::size_type idx;
    // Find beginning of the command word
    idx = line.find_first_not_of(delims);
    // Find end of the command word
    idx = line.find_first_of(delims, idx);
    // Find beginning of param
    idx = line.find_first_not_of(delims, idx);
    if (idx == string::npos)
        return string();
    string param = line.substr(idx);
    // trim spaces off the end
    param = param.substr(0, param.find_last_not_of(delims) + 1);
    // replace escaped quotes
    str_replace(param, "\\\"", "\"");

    if (firstWord)
    {
        idx = param.find_first_of(delims);
        if (idx != string::npos)
            param = param.substr(0, idx);
    }
    return param;
}

static REFERENCE_TIME ParseCueIndex(string line)
{
    int index, m, s, f, ret;
    ret = sscanf_s(line.c_str(), " INDEX %d %d:%d:%d", &index, &m, &s, &f);
    if (ret == 4)
        return (m * 60i64 + s) * 10000000i64 + (f * 10000000i64 / 75);
    else
        return 0;
}

CCueSheet::CCueSheet()
{
}

CCueSheet::~CCueSheet()
{
}

HRESULT CCueSheet::Parse(string cueSheet)
{
    DbgLog((LOG_TRACE, 10, L"CCueSheet::Parse(): Parsing Cue Sheet"));
    int trackCount = 0;
    ParserState state(ParserState::GLOBAL);
    stringstream cueSheetStream(cueSheet);
    string line;
    while (getline(cueSheetStream, line))
    {
        string word;
        (stringstream(line)) >> word;
        word = toupper(word);
        switch (state)
        {
        case ParserState::GLOBAL:
            if (word == "PERFORMER")
            {
                m_Performer = GetCueParam(line);
            }
            else if (word == "TITLE")
            {
                m_Title = GetCueParam(line);
            }
            else if (word == "FILE")
            {
                state = ParserState::FILE;
            }
            break;
        case ParserState::FILE:
        case ParserState::TRACK:
            if (word == "FILE")
            {
                DbgLog((LOG_TRACE, 10, L"CCueSheet::Parse(): Multiple FILE segments not supported."));
                return E_FAIL;
            }
            if (word == "TRACK")
            {
                state = ParserState::TRACK;
                trackCount++;

                string id = GetCueParam(line, true);
                Track track{trackCount - 1, id, "Title " + id, 0, ""};
                m_Tracks.push_back(track);
            }
            else if (state == ParserState::TRACK)
            {
                if (word == "TITLE")
                {
                    m_Tracks.back().Title = GetCueParam(line);
                }
                else if (word == "INDEX")
                {
                    m_Tracks.back().Time = ParseCueIndex(line);
                }
                else if (word == "PERFORMER")
                {
                    m_Tracks.back().Performer = GetCueParam(line);
                }
            }
            break;
        }
    }

    return S_OK;
}

std::string CCueSheet::FormatTrack(Track &track)
{
    string trackFormat = track.Id + ". ";
    if (!track.Performer.empty())
        trackFormat += track.Performer + " - ";
    trackFormat += track.Title;
    return trackFormat;
}
