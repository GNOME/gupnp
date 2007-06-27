/* 
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __UPNP_PROTOCOL_H__
#define __UPNP_PROTOCOL_H__

#include "gupnp-error.h"

G_BEGIN_DECLS

struct {
        const char *code;
        const char *description;
} upnp_errors[GUPNP_ACTION_ERROR_LAST] = {
        { "401", "Invalid Action" },
        { "402", "Invalid Args"   },
        { "403", "Out of Sync"    },
        { "501", "Action Failed"  }
};

G_END_DECLS

#endif /* __UPNP_PROTOCOL_H__ */
