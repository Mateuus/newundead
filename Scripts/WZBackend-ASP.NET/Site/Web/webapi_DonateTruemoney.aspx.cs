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

public partial class Web_webapi_DonateTruemoney : WOApiWebPage
{
    protected override void Execute()
    {
        string CustomerID = web.CustomerID();
        string GC = web.Param("gc");
        string Item = web.Param("item");
        string ItemAmount = web.Param("itemamount");
        string CardNo = web.Param("cardno");

        SaveApiLog("webapi_DonateTruemoney.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Donate_Truemoney";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_GC", GC);
        sqcmd.Parameters.AddWithValue("@in_Item", Item);
        sqcmd.Parameters.AddWithValue("@in_ItemAmount", ItemAmount);
        sqcmd.Parameters.AddWithValue("@in_CardNo", CardNo);

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
