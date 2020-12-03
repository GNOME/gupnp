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

#include <libgupnp/gupnp-acl.h>
#include <libgupnp/gupnp-context.h>
#include <libgupnp/gupnp-context-manager.h>
#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-error.h>
#include <libgupnp/gupnp-device.h>
#include <libgupnp/gupnp-device-info.h>
#include <libgupnp/gupnp-device-proxy.h>
#include <libgupnp/gupnp-resource-factory.h>
#include <libgupnp/gupnp-root-device.h>
#include <libgupnp/gupnp-service.h>
#include <libgupnp/gupnp-service-info.h>
#include <libgupnp/gupnp-service-introspection.h>
#include <libgupnp/gupnp-service-proxy.h>
#include <libgupnp/gupnp-white-list.h>
#include <libgupnp/gupnp-xml-doc.h>
#include <libgupnp/gupnp-types.h>
#include <libgupnp/gupnp-uuid.h>
