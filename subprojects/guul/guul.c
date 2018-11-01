/*
 * GUUL - GUUL Unified UUID Library
 * Copyright (C) 2015 Jens Georg.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>

#include <guul.h>

#if defined(GUUL_PLATFORM_GENERIC) || defined(GUUL_PLATFORM_BSD)
#   include <uuid.h>
#endif

#if defined(GUUL_PLATFORM_OSX)
#   include <uuid/uuid.h>
#endif

#if defined(GUUL_PLATFORM_WINDOWS)
#    include <rpc.h>
#endif

#if defined(GUUL_PLATFORM_GENERIC) || defined(GUUL_PLATFORM_OSX)
char *
guul_get_uuid(void)
{
        uuid_t uuid;
        char *out;

        out = calloc (41, sizeof (char));

        uuid_generate (uuid);
        uuid_unparse (uuid, out);

        return out;
}
#endif

#if defined(GUUL_PLATFORM_BSD)
char *
guul_get_uuid()
{
        uuid_t uuid;
        uint32_t status;
        char *result = NULL;

        uuid_create (&uuid, &status);
        uuid_to_string (&uuid, &result, &status);

        return result;
}
#endif

#if defined(GUUL_PLATFORM_WINDOWS)
char *
guul_get_uuid()
{
    char *ret = NULL;
    UUID uuid;
    RPC_STATUS stat;
    stat = UuidCreate (&uuid);
    if (stat == RPC_S_OK) {
            unsigned char* uuidStr = NULL;
            stat = UuidToString (&uuid, &uuidStr);
            if (stat == RPC_S_OK) {
                    ret = g_strdup (uuidStr);
                    RpcStringFree (&uuidStr);
            }
    }

    return ret;
}
#endif
