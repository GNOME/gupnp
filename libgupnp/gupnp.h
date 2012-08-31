/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gupnp-context.h"
#include "gupnp-context-manager.h"
#include "gupnp-control-point.h"
#include "gupnp-error.h"
#include "gupnp-device.h"
#include "gupnp-device-info.h"
#include "gupnp-device-proxy.h"
#include "gupnp-resource-factory.h"
#include "gupnp-root-device.h"
#include "gupnp-service.h"
#include "gupnp-service-info.h"
#include "gupnp-service-introspection.h"
#include "gupnp-service-proxy.h"
#include "gupnp-xml-doc.h"
#include "gupnp-types.h"
