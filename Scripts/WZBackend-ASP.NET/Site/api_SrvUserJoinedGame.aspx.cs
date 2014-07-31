using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_SrvUserJoinedGame : WOApiWebPage
{
    protected override void Execute()
    {
        // disconnect if we don't have correct credentials
        if (!WoCheckLoginSession())
            return;

        string skey1 = web.Param("skey1");
        if (skey1 != SERVER_API_KEY)
            throw new ApiExitException("bad key");

        string CustomerID = web.CustomerID();

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_UserJoinedGame2";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_CharID", web.Param("CharID"));
        sqcmd.Parameters.AddWithValue("@in_GameMapId", web.Param("g1"));
        sqcmd.Parameters.AddWithValue("@in_GameServerId", web.Param("g2"));
        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }
}
