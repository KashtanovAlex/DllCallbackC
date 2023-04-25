/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Errors.h"
#include "Log.h"
#include "GitRevision.h"
#include "DllLoadMgr.h"
#include <csignal>

int main()
{
    signal(SIGABRT, &Warhead::AbortHandler);

    // Init logging system
    sLog->UsingDefaultLogs();

    LOG_INFO("armlib", "{}", GitRevision::GetFullVersion());

    // Check dynamic lib
    sDllMgr->TestDll("C:\\Users\\kashtanov\\Desktop\\ArmDllTest-repo\\build\\bin\\RelWithDebInfo\\script_arm.dll");

    LOG_INFO("checker", "Halting process...");

    // 0 - normal shutdown
    // 1 - shutdown at error
    return 0;
}
