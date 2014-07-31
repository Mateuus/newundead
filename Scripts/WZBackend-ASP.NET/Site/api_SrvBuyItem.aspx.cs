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

public partial class api_SrvBuyItem : WOApiWebPage
{
    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

        string CustomerID = web.CustomerID();
        string ItemID = web.Param("ItemID");
        string Amount = web.Param("Amount");
        string BuyIdx = web.Param("BuyIdx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SrvBuyItemToChar";
   
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_ItemId", ItemID);
        sqcmd.Parameters.AddWithValue("@in_Amount", Amount);
        sqcmd.Parameters.AddWithValue("@in_BuyIdx", BuyIdx);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }
}
