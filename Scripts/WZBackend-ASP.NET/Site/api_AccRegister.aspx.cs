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

public partial class api_AccRegister : WOApiWebPage
{
    protected override void Execute()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_ACCOUNT_CREATE";
        sqcmd.Parameters.AddWithValue("@in_IP", LastIP);
        sqcmd.Parameters.AddWithValue("@in_EMail", web.Param("username")); // login name from updater
        sqcmd.Parameters.AddWithValue("@in_Password", web.Param("password"));
        sqcmd.Parameters.AddWithValue("@in_ReferralID", 0);
        sqcmd.Parameters.AddWithValue("@in_SerialKey", web.Param("serial"));
        sqcmd.Parameters.AddWithValue("@in_SerialEmail", web.Param("email"));   // email used in serial key purchase

        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        int CustomerID = getInt("CustomerID");

        Response.Write("WO_0");
        Response.Write(string.Format("{0}", CustomerID));
    }
}
