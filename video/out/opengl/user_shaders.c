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

#include "user_shaders.h"

// Returns false if no more shaders could be parsed
bool parse_user_shader_pass(struct mp_log *log, struct bstr *body,
                            struct gl_user_shader *out)
{
    if (!body || !out || !body->start || body->len == 0)
        return false;

    *out = (struct gl_user_shader){ .transform = identity_trans };
    int hook_idx = 0;
    int bind_idx = 0;

    // First parse all the headers
    while (true) {
        struct bstr rest;
        struct bstr line = bstr_getline(*body, &rest);

        // Check for the presence of the magic line beginning
        if (!bstr_eatstart0(&line, "//!"))
            break;

        *body = rest;

        // Parse the supported commands
        if (bstr_eatstart0(&line, "HOOK")) {
            if (hook_idx == SHADER_MAX_HOOKS) {
                mp_err(log, "Passes may only hook up to %d textures!\n",
                       SHADER_MAX_HOOKS);
                return false;
            }
            out->hook_tex[hook_idx++] = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "BIND")) {
            if (bind_idx == SHADER_MAX_BINDS) {
                mp_err(log, "Passes may only bind up to %d textures!\n",
                       SHADER_MAX_BINDS);
                return false;
            }
            out->bind_tex[bind_idx++] = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "SAVE")) {
            out->save_tex = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "TRANSFORM")) {
            float sx, sy, ox, oy;
            if (bstr_sscanf(line, "%f %f %f %f", &sx, &sy, &ox, &oy) != 4) {
                mp_err(log, "Error while parsing TRANSFORM!\n");
                return false;
            }
            out->transform = (struct gl_transform){{{sx, 0}, {0, sy}}, {ox, oy}};
            continue;
        }

        if (bstr_eatstart0(&line, "COMPONENTS")) {
            if (bstr_sscanf(line, "%d", &out->components) != 1) {
                mp_err(log, "Error while parsing COMPONENTS!\n");
                return false;
            }
            continue;
        }

        // Unknown command type
        char *str = bstrto0(NULL, line);
        mp_err(log, "Unrecognized command '%s'!\n", str);
        talloc_free(str);
        return false;
    }

    // The rest of the file up until the next magic line beginning (if any)
    // shall be the shader body
    if (bstr_split_tok(*body, "//!", &out->pass_body, body)) {
        // Make sure the magic line is part of the rest
        body->start -= 3;
        body->len += 3;
    }

    // Sanity checking
    if (hook_idx == 0)
        mp_warn(log, "Pass has no hooked textures (will be ignored)!\n");

    return true;
}
