--[[ The MIT License (MIT)

Copyright (c) 2020 Seth Warn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. ]]

-- The lua implementation of the fzy string matching algorithm

local SCORE_GAP_LEADING = -0.005
local SCORE_GAP_TRAILING = -0.005
local SCORE_GAP_INNER = -0.01
local SCORE_MATCH_CONSECUTIVE = 1.0
local SCORE_MATCH_SLASH = 0.9
local SCORE_MATCH_WORD = 0.8
local SCORE_MATCH_CAPITAL = 0.7
local SCORE_MATCH_DOT = 0.6
local SCORE_MAX = math.huge
local SCORE_MIN = -math.huge
local MATCH_MAX_LENGTH = 1024

local fzy = {}

local byte = string.byte

-- Reusable memory arrays for repeating filtering calls. Profiling shown that
-- with huge payloads we spend a lot of time allocating and later dealociting,
-- making poor GC work overtime. Keep them allocated. Free them with fzy.gc()
-- after use.
local match_bonus, row_D1, row_M1, row_D2, row_M2 = {}, {}, {}, {}, {}

function fzy.gc()
  match_bonus, row_D1, row_M1, row_D2, row_M2 = {}, {}, {}, {}, {}
end

-- Check if `needle` is a subsequence of the `haystack`.
--
-- Usually called before `score` or `positions`.
--
-- Args:
--   needle (string)
--   haystack (string)
--   case_sensitive (bool, optional): defaults to false
--
-- Returns:
--   bool
function fzy.has_match(needle, haystack, case_sensitive)
  if not case_sensitive then
    needle = string.lower(needle)
    haystack = string.lower(haystack)
  end

  local j = 1
  for i = 1, string.len(needle) do
    j = string.find(haystack, needle:sub(i, i), j, true)
    if not j then
      return false
    else
      j = j + 1
    end
  end

  return true
end

local bonus_other = {
  [byte("/")]  = SCORE_MATCH_SLASH,
  [byte("\\")] = SCORE_MATCH_SLASH,
  [byte("-")]  = SCORE_MATCH_WORD,
  [byte("_")]  = SCORE_MATCH_WORD,
  [byte(" ")]  = SCORE_MATCH_WORD,
  [byte(".")]  = SCORE_MATCH_DOT,
}
local bonus_upper, bonus_state = {}, {}
for c = 0, 255 do
  bonus_other[c] = bonus_other[c] or 0
  bonus_upper[c] = c >= byte("a") and c <= byte("z") and SCORE_MATCH_CAPITAL or bonus_other[c]
  bonus_state[c] = c >= byte("A") and c <= byte("Z") and bonus_upper or bonus_other
end

local function precompute_bonus(haystack)
  local last_char = byte("/")
  for i = 1, #haystack do
    local this_char = byte(haystack, i)
    match_bonus[i] = bonus_state[this_char][last_char]
    last_char = this_char
  end

  return match_bonus
end



-- Run the dynamic programming loop over the score matrices D (score of the
-- best match ending at this position) and M (best score up to this position).
--
-- `needle` and `haystack` must already be case-folded as desired and both
-- non-empty, with #needle < #haystack <= MATCH_MAX_LENGTH. `haystack_cased`
-- is the original-case haystack, used for the match bonuses.
--
-- Returns the score, and, if `full` is true, also the D and M matrices as
-- freshly allocated arrays of rows, which are needed to trace back the
-- matched positions. Otherwise only two reused rows are kept, making
-- repeated scoring allocation-free.
local function compute(needle, haystack_cased, haystack, full)
  local n = #needle
  local m = #haystack


  -- Note that the match bonuses must be computed before the arguments are
  -- converted to lowercase, since there are bonuses for camelCase.
  local bonus = precompute_bonus(haystack_cased)

  local D, M
  if full then
    D, M = {}, {}
  end

  local prev_D, prev_M, cur_D, cur_M = row_D2, row_M2, row_D1, row_M1

  for i = 1, n do
    if full then
      cur_D, cur_M = {}, {}
      D[i], M[i] = cur_D, cur_M
    end

    local prev_score = SCORE_MIN
    local gap_score = i == n and SCORE_GAP_TRAILING or SCORE_GAP_INNER
    local needle_char = byte(needle, i)

    for j = 1, m do
      if needle_char == byte(haystack, j) then
        local score = SCORE_MIN
        if i == 1 then
          score = (j - 1) * SCORE_GAP_LEADING + bonus[j]
        elseif j > 1 then
          local match = prev_M[j - 1] + bonus[j]
          local consecutive = prev_D[j - 1] + SCORE_MATCH_CONSECUTIVE
          score = match > consecutive and match or consecutive
        end
        cur_D[j] = score
        prev_score = prev_score + gap_score
        if score > prev_score then
          prev_score = score
        end
        cur_M[j] = prev_score
      else
        cur_D[j] = SCORE_MIN
        prev_score = prev_score + gap_score
        cur_M[j] = prev_score
      end
    end

    prev_D, prev_M, cur_D, cur_M = cur_D, cur_M, prev_D, prev_M
  end

  return prev_M[m], D, M
end

-- Compute a matching score.
--
-- Args:
--   needle (string): must be a subsequence of `haystack`, or the result is
--     undefined.
--   haystack (string)
--   case_sensitive (bool, optional): defaults to false
--
-- Returns:
--   number: higher scores indicate better matches. See also `get_score_min`
--     and `get_score_max`.
function fzy.score(needle, haystack, case_sensitive)
  local n = #needle
  local m = #haystack

  if n == 0 or m == 0 or m > MATCH_MAX_LENGTH or n > m then
    return SCORE_MIN
  elseif n == m then
    return SCORE_MAX
  end

  local haystack_cased = haystack
  if not case_sensitive then
    needle = string.lower(needle)
    haystack = string.lower(haystack)
  end

  local score = compute(needle, haystack_cased, haystack, false)
  return score
end

-- Compute the locations where fzy matches a string.
--
-- Determine where each character of the `needle` is matched to the `haystack`
-- in the optimal match.
--
-- Args:
--   needle (string): must be a subsequence of `haystack`, or the result is
--     undefined.
--   haystack (string)
--   case_sensitive (bool, optional): defaults to false
--
-- Returns:
--   {int,...}: indices, where `indices[n]` is the location of the `n`th
--     character of `needle` in `haystack`.
--   number: the same matching score returned by `score`
function fzy.positions(needle, haystack, case_sensitive)
  local n = #needle
  local m = #haystack

  if n == 0 or m == 0 or m > MATCH_MAX_LENGTH or n > m then
    return {}, SCORE_MIN
  elseif n == m then
    local consecutive = {}
    for i = 1, n do
      consecutive[i] = i
    end
    return consecutive, SCORE_MAX
  end

  local haystack_cased = haystack
  if not case_sensitive then
    needle = string.lower(needle)
    haystack = string.lower(haystack)
  end

  local score, D, M = compute(needle, haystack_cased, haystack, true)

  local positions = {}
  local match_required = false
  local j = m
  for i = n, 1, -1 do
    while j >= 1 do
      if D[i][j] ~= SCORE_MIN and (match_required or D[i][j] == M[i][j]) then
        match_required = (i ~= 1) and (j ~= 1) and (
        M[i][j] == D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE)
        positions[i] = j
        j = j - 1
        break
      else
        j = j - 1
      end
    end
  end

  return positions, score
end

-- Apply `has_match` and `positions` to an array of haystacks.
--
-- Args:
--   needle (string)
--   haystack ({string, ...})
--   case_sensitive (bool, optional): defaults to false
--
-- Returns:
--   {{idx, positions, score}, ...}: an array with one entry per matching line
--     in `haystacks`, each entry giving the index of the line in `haystacks`
--     as well as the equivalent to the return value of `positions` for that
--     line.
function fzy.filter(needle, haystacks, case_sensitive)
  local result = {}

  for i, line in ipairs(haystacks) do
    if fzy.has_match(needle, line, case_sensitive) then
      local p, s = fzy.positions(needle, line, case_sensitive)
      table.insert(result, {i, p, s})
    end
  end

  return result
end

-- Filter a range of haystacks, computing only scores, without positions.
--
-- This can be called on unfinished over consecutive ranges to allow processing
-- pending input between the calls.
--
-- Args:
--   needle (string)
--   haystacks ({string, ...})
--   first (int), last (int): inclusive range of haystacks to process
--   indices ({int, ...}): the indices of matching haystacks are appended
--   scores ({number, ...}): the matches' scores are appended, parallel to
--     `indices`
--   case_sensitive (bool, optional): defaults to false
--   haystacks_lower ({string, ...} or nil): string.lower() of each haystack,
--     used when not case_sensitive instead of allocating lowercased copies
--     on every call
function fzy.filter_range(needle, haystacks, first, last, indices, scores,
                          case_sensitive, haystacks_lower)
  local find = string.find
  local lower = string.lower

  if not case_sensitive then
    needle = lower(needle)
  end

  local n = #needle
  local needle_chars = {}
  for i = 1, n do
    needle_chars[i] = needle:sub(i, i)
  end

  local k = #indices

  for i = first, last do
    local haystack = haystacks[i]
    local folded = haystack
    if not case_sensitive then
      folded = haystacks_lower and haystacks_lower[i] or lower(haystack)
    end

    local j = 1
    for ci = 1, n do
      j = find(folded, needle_chars[ci], j, true)
      if not j then
        break
      end
      j = j + 1
    end

    if j then
      local m = #haystack
      k = k + 1
      indices[k] = i
      if n == 0 or m > MATCH_MAX_LENGTH then
        scores[k] = SCORE_MIN
      elseif n == m then
        scores[k] = SCORE_MAX
      else
        local score = compute(needle, haystack, folded, false)
        scores[k] = score
      end
    end
  end
end

-- The lowest value returned by `score`.
--
-- In two special cases:
--  - an empty `needle`, or
--  - a `needle` or `haystack` larger than than `get_max_length`,
-- the `score` function will return this exact value, which can be used as a
-- sentinel. This is the lowest possible score.
function fzy.get_score_min()
  return SCORE_MIN
end

-- The score returned for exact matches. This is the highest possible score.
function fzy.get_score_max()
  return SCORE_MAX
end

-- The maximum size for which `fzy` will evaluate scores.
function fzy.get_max_length()
  return MATCH_MAX_LENGTH
end

-- The minimum score returned for normal matches.
--
-- For matches that don't return `get_score_min`, their score will be greater
-- than than this value.
function fzy.get_score_floor()
  return MATCH_MAX_LENGTH * SCORE_GAP_INNER
end

-- The maximum score for non-exact matches.
--
-- For matches that don't return `get_score_max`, their score will be less than
-- this value.
function fzy.get_score_ceiling()
  return MATCH_MAX_LENGTH * SCORE_MATCH_CONSECUTIVE
end

-- The name of the currently-running implementation, "lua" or "native".
function fzy.get_implementation_name()
  return "lua"
end

return fzy
