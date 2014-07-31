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

public partial class api_BanID : WOApiWebPage
{
    void OutfitOp(string CustomerID, string reason)
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "ADMIN_BanUser";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_Reason", reason);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        //if (!WoCheckLoginSession())
         //   return;
        string CustomerID = web.Param("CustomerID");
        string reason = web.Param("reason");

        OutfitOp(CustomerID,reason);
    }
}