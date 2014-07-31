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

public partial class api_BuyPremium : WOApiWebPage
{
    void OutfitOp(string CustomerID)
    {
        SaveApiLog("api_BuyPremium.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_ACCOUNT_BUYPREMIUM";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;
        string CustomerID = web.Param("CustomerID");

        OutfitOp(CustomerID);
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