using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.Configuration;

public partial class Web_webapi_BanPlayer : WOApiWebPage
{
    protected override void Execute()
    {
        string skey1 = web.Param("skey1");
        if (skey1 != SERVER_API_KEY)
            throw new ApiExitException("bad key");

        string CustomerID = web.CustomerID();
        string BanReason = web.Param("reason");

        SaveApiLog("webapi_BanPlayer.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_ADMIN_BanUser";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_BanReason", BanReason);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    private void SaveApiLog(string apiname)
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_ApiLog";
        sqcmd.Parameters.AddWithValue("@in_IP", Request.UserHostAddress);
        sqcmd.Parameters.AddWithValue("@in_ApiName", apiname);
        sqcmd.Parameters.AddWithValue("@in_QueryString", Request.Url.Query);

        if (!CallWOApi(sqcmd))
            return;
    }
}
