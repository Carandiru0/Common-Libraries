/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#pragma once

 /* no windows.h includes here - include this file after windows.h or equivalent has been included in the file this functionailty is required in instead ! */


/*++
Routine Description: This routine returns TRUE if the caller's
process is a member of the Administrators local group. Caller is NOT
expected to be impersonating anyone and is expected to be able to
open its own process and process token.
Arguments: None.
Return Value:
   TRUE - Caller has Administrators local group.
   FALSE - Caller does not have Administrators local group. --
*/
static bool const isUserAdmin()
{
	constinit static bool isAdminChecked(false),
						  isAdminResult(false);					
	
	if (isAdminChecked) {
		return(isAdminResult);
	}
	else {
		BOOL b;
		SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
		PSID AdministratorsGroup;
		b = AllocateAndInitializeSid(
			&NtAuthority,
			2,
			SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS,
			0, 0, 0, 0, 0, 0,
			&AdministratorsGroup);
		if (b)
		{
			if (!CheckTokenMembership(NULL, AdministratorsGroup, &b))
			{
				b = FALSE;
			}
			FreeSid(AdministratorsGroup);
		}
		
		isAdminChecked = true;
		isAdminResult = (bool)b;
	}
	
	return(isAdminResult);
}