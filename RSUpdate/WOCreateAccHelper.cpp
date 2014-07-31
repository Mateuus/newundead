#include "r3dPCH.h"
#include "r3d.h"

#include "WOCreateAccHelper.h"
#include "WOBackendAPI.h"

int CCreateAccHelper::DoCreateAcc()
{
	r3d_assert(createAccCode == CA_Unactive);
	r3d_assert(*username);
	r3d_assert(*passwd1);
	r3d_assert(*passwd2);
	//r3d_assert(*serial);

	CWOBackendReq req("api_AccRegister.aspx");
	req.AddParam("username", username);
	req.AddParam("password", passwd1);
	req.AddParam("serial",   "0");
	req.AddParam("email",    "not@used.anymore"); // previously that was order email.

	createAccCode = CA_Processing;
	req.Issue();
        createAccCode = CCreateAccHelper::CA_Unactive;

	return req.resultCode_;
}
