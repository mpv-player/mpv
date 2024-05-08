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

local function is_lower(c)
  return c:match("%l")
end

local function is_upper(c)
  return c:match("%u")
end

local function precompute_bonus(haystack)
  local match_bonus = {}

  local last_char = "/"
  for i = 1, string.len(haystack) do
    local this_char = haystack:sub(i, i)
    if last_char == "/" or last_char == "\\" then
      match_bonus[i] = SCORE_MATCH_SLASH
    elseif last_char == "-" or last_char == "_" or last_char == " " then
      match_bonus[i] = SCORE_MATCH_WORD
    elseif last_char == "." then
      match_bonus[i] = SCORE_MATCH_DOT
    elseif is_lower(last_char) and is_upper(this_char) then
      match_bonus[i] = SCORE_MATCH_CAPITAL
    else
      match_bonus[i] = 0
    end

    last_char = this_char
  end

  return match_bonus
end

local function compute(needle, haystack, D, M, case_sensitive)
  -- Note that the match bonuses must be computed before the arguments are
  -- converted to lowercase, since there are bonuses for camelCase.
  local match_bonus = precompute_bonus(haystack)
  local n = string.len(needle)
  local m = string.len(haystack)

  if not case_sensitive then
    needle = string.lower(needle)
    haystack = string.lower(haystack)
  end

  -- Because lua only grants access to chars through substring extraction,
  -- get all the characters from the haystack once now, to reuse below.
  local haystack_chars = {}
  for i = 1, m do
    haystack_chars[i] = haystack:sub(i, i)
  end

  for i = 1, n do
    D[i] = {}
    M[i] = {}

    local prev_score = SCORE_MIN
    local gap_score = i == n and SCORE_GAP_TRAILING or SCORE_GAP_INNER
    local needle_char = needle:sub(i, i)

    for j = 1, m do
      if needle_char == haystack_chars[j] then
        local score = SCORE_MIN
        if i == 1 then
          score = ((j - 1) * SCORE_GAP_LEADING) + match_bonus[j]
        elseif j > 1 then
          local a = M[i - 1][j - 1] + match_bonus[j]
          local b = D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE
          score = math.max(a, b)
        end
        D[i][j] = score
        prev_score = math.max(score, prev_score + gap_score)
        M[i][j] = prev_score
      else
        D[i][j] = SCORE_MIN
        prev_score = prev_score + gap_score
        M[i][j] = prev_score
      end
    end
  end
end

-- Compute a matching score.
--
-- Args:
--   needle (string): must be a subequence of `haystack`, or the result is
--     undefined.
--   haystack (string)
--   case_sensitive (bool, optional): defaults to false
--
-- Returns:
--   number: higher scores indicate better matches. See also `get_score_min`
--     and `get_score_max`.
function fzy.score(needle, haystack, case_sensitive)
  local n = string.len(needle)
  local m = string.len(haystack)

  if n == 0 or m == 0 or m > MATCH_MAX_LENGTH or n > m then
    return SCORE_MIN
  elseif n == m then
    return SCORE_MAX
  else
    local D = {}
    local M = {}
    compute(needle, haystack, D, M, case_sensitive)
    return M[n][m]
  end
end

-- Compute the locations where fzy matches a string.
--
-- Determine where each character of the `needle` is matched to the `haystack`
-- in the optimal match.
--
-- Args:
--   needle (string): must be a subequence of `haystack`, or the result is
--     undefined.
--   haystack (string)
--   case_sensitive (bool, optional): defaults to false
--
-- Returns:
--   {int,...}: indices, where `indices[n]` is the location of the `n`th
--     character of `needle` in `haystack`.
--   number: the same matching score returned by `score`
function fzy.positions(needle, haystack, case_sensitive)
  local n = string.len(needle)
  local m = string.len(haystack)

  if n == 0 or m == 0 or m > MATCH_MAX_LENGTH or n > m then
    return {}, SCORE_MIN
  elseif n == m then
    local consecutive = {}
    for i = 1, n do
      consecutive[i] = i
    end
    return consecutive, SCORE_MAX
  end

  local D = {}
  local M = {}
  compute(needle, haystack, D, M, case_sensitive)

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

  return positions, M[n][m]
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

-- The name of the currently-running implmenetation, "lua" or "native".
function fzy.get_implementation_name()
  return "lua"
end

return fzy
