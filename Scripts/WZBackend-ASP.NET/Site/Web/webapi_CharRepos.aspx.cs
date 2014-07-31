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
using System.Text.RegularExpressions;

public partial class Web_webapi_CharRepos : WOApiWebPage
{
    protected override void Execute()
    {
        string CustomerID = web.CustomerID();
        string SessionID = web.SessionKey();
        string CharID = web.Param("CharID");
        int inPositionNo = int.Parse(web.Param("PositionNo"));

        string PositionNo = "";
        if (inPositionNo == 1)
        {
            PositionNo = "1";
        }
        else if (inPositionNo == 2)
        {
            PositionNo = "2";
        }
        else
        {
            Response.Write("WO_4");
            Response.Write("PositionNo is not Valid");
            Response.Write("<br/>PositionNo is " + inPositionNo);
            return;
        }

        SaveApiLog("webapi_CharRepos.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Character_RePosition";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_SessionID", SessionID);
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_PositionNO", PositionNo);

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
