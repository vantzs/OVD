/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechavalier <david@ulteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#ifndef foomodulerdpsinksymdeffoo
#define foomodulerdpsinksymdeffoo

#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/macro.h>

#define pa__init module_rdp_sink_LTX_pa__init
#define pa__done module_rdp_sink_LTX_pa__done
#define pa__get_author module_rdp_sink_LTX_pa__get_author
#define pa__get_description module_rdp_sink_LTX_pa__get_description
#define pa__get_usage module_rdp_sink_LTX_pa__get_usage
#define pa__get_version module_rdp_sink_LTX_pa__get_version
#define pa__load_once module_rdp_sink_LTX_pa__load_once

int pa__init(pa_module*m);
void pa__done(pa_module*m);

const char* pa__get_author(void);
const char* pa__get_description(void);
const char* pa__get_usage(void);
const char* pa__get_version(void);
pa_bool_t pa__load_once(void);

#endif
